/**
 * @file SerialDataSource.cpp
 * @brief Cross-platform serial port implementation (see SerialDataSource.hpp).
 *
 * @details
 * The file is split into `#if` blocks per operating system:
 *  - `_WIN32` / `_WIN64`: Win32 API (QueryDosDevice, CreateFileA, DCB, COMMTIMEOUTS);
 *  - `__linux__`: termios with the standard Bxxxx constants;
 *  - `__APPLE__`: termios + IOSSIOSPEED ioctl for arbitrary baud rates.
 *
 * To support a new baud rate add it both to the `switch` of the relevant
 * platform in open() and to supportedBaudRates().
 */
#include <iostream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <Telemetry/data_acquisition/SerialDataSource.hpp>

#if defined (_WIN32) || defined( _WIN64)
    #include <Windows.h>
#endif
#if defined(__APPLE__) || defined(__linux__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <stdio.h>
    #include <regex>
    #include <sys/ioctl.h>
#endif
#if defined(__APPLE__)
    #include <IOKit/serial/ioss.h>
#endif
#include <thread>

SerialDataSource::SerialDataSource()
#if defined (_WIN32) || defined( _WIN64) 
    : handle(nullptr)
#else
    : fd(-1)
#endif
{
}

SerialDataSource::~SerialDataSource()
{
  close();
}

std::vector<std::string> SerialDataSource::getAvailableResources()
{
    // Windows: tries the names COM0..COM254 and keeps the ones that exist.
    // Note: on Windows the list is NOT cleared before scanning.
#if defined (_WIN32) || defined( _WIN64)
	const uint32_t CHAR_NUM = 1024;
	const uint32_t MAX_PORTS = 255;
	const std::string COM_STR = "COM";
	char path[CHAR_NUM];
	for (uint32_t k = 0; k < MAX_PORTS; k++)
	{
		std::string port_name = COM_STR + std::to_string(k);
		DWORD test = QueryDosDeviceA(port_name.c_str(), path, CHAR_NUM);
		if (test == 0) continue;
		m_availablePorts.push_back(port_name);
	}
#endif
#if defined(__linux__)
    namespace fs = std::filesystem;
    m_availablePorts.clear();
    try {
        const std::string DEV_PATH = "/dev";
        if (!fs::exists(DEV_PATH)) return m_availablePorts;
        
        for (const auto& entry : fs::directory_iterator(DEV_PATH)) {
            std::string name = entry.path().filename().string();
            // Typical Linux serial devices: USB adapters, CDC-ACM, native UARTs, Bluetooth
            if (name.find("ttyUSB") == 0 || 
                name.find("ttyACM") == 0 || 
                name.find("ttyS") == 0 || 
                name.find("rfcomm") == 0) {
                m_availablePorts.push_back(entry.path().string());
            }
        }
    } catch (const fs::filesystem_error &ex) {}
#endif
    // macOS: keeps the /dev/tty.* and /dev/cu.* devices (after resolving symbolic links)
#if defined(__APPLE__)
	namespace fs = std::filesystem;
	const std::string DEV_PATH = "/dev";
	const std::regex base_regex(R"(\/dev\/(tty|cu)\..*)");
    m_availablePorts.clear();
    try
    {
        fs::path p(DEV_PATH);
        if (!fs::exists(DEV_PATH)) return m_availablePorts;
        for (fs::directory_entry de: fs::directory_iterator(p)) {
            fs::path canonical_path = fs::canonical(de);
            std::string name = canonical_path.generic_string();
            std::smatch res;
            std::regex_search(name, res, base_regex);
            if (res.empty()) continue;
            m_availablePorts.push_back(canonical_path.generic_string());
        }
    }
    catch (const fs::filesystem_error &ex) {}
#endif
	std::sort(m_availablePorts.begin(), m_availablePorts.end());
	return m_availablePorts;
}

bool SerialDataSource::open(const std::string &resource, int baudRate)
{
    // ---------------- Windows ----------------
#if defined (_WIN32) || defined( _WIN64)
    handle = CreateFileA(resource.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) {
        std::cerr << "ERROR [SerialDataSource]: Failed to open serial port: " << resource.c_str() << "." << std::endl;
        handle = nullptr;
        return false;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if(!GetCommState(handle, &dcbSerialParams)) {
        std::cerr << "ERROR [SerialDataSource]: Failed to get current serial parameters." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

    // Baud rate -> Win32 constant (500000 has no CBR_ constant, so the value is passed as is)
    DWORD speed;
    switch(baudRate) {
        case 300: speed = CBR_300; break;
        case 600: speed = CBR_600; break;
        case 1200: speed = CBR_1200; break;
        case 2400: speed = CBR_2400; break;
        case 4800: speed = CBR_4800; break;
        case 9600: speed = CBR_9600; break;
        case 19200: speed = CBR_19200; break;
        case 38400: speed = CBR_38400; break;
        case 57600: speed = CBR_57600; break;
        case 115200: speed = CBR_115200; break;
        case 128000: speed = CBR_128000; break;
        case 256000: speed = CBR_256000; break;
        case 500000: speed = 500000; break;
        default:
            std::cerr << "ERROR [SerialDataSource]: Baud rate not supported: " << baudRate << "." << std::endl;
            CloseHandle(handle);
            handle = nullptr;
            return false;
    }

    // 8N1 configuration: 8 data bits, no parity, 1 stop bit
    dcbSerialParams.BaudRate = speed;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if(!SetCommState(handle, &dcbSerialParams)) {
        std::cerr << "ERROR [SerialDataSource]: Failed to set serial port parameters." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

    // Timeouts set for NON-blocking reads: ReadFile returns immediately with
    // whatever is in the buffer (even 0 bytes)
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = MAXDWORD; // returns immediately
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if(!SetCommTimeouts(handle, &timeouts)) {
        std::cerr << "ERROR [SerialDataSource]: Failed to set serial port timeouts." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

    // Discard any data already pending in the input buffer: it would cause stale
    // data and false packet loss at startup
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return true;
#endif
    // ---------------- Linux / macOS (termios) ----------------
#if defined(__APPLE__) || defined(__linux__)
    fd = ::open(resource.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if(fd < 0) {
        std::cerr << "ERROR [SerialDataSource]: Failed to open serial port." << std::endl;
        return false;
    }
    
    // Non-blocking mode: read() returns immediately even without data
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct termios tty{};
    if(tcgetattr(fd, &tty) != 0) {
        std::cerr << "ERROR [SerialDataSource]: Failed to get serial port parameters." << std::endl;
        ::close(fd); fd = -1;
        return false;
    }

    // Raw 8N1 configuration: 8 bits, no parity, 1 stop bit, no flow control
    // (neither software XON/XOFF nor hardware RTS/CTS), no character
    // processing, VMIN=VTIME=0 (non-blocking read)
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     
    tty.c_iflag &= ~IGNBRK;         
    tty.c_lflag = 0;                
    tty.c_oflag = 0;                
    tty.c_cc[VMIN]  = 0;            
    tty.c_cc[VTIME] = 0;            
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); 
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);      
    tty.c_cflag &= ~CSTOPB; 
    tty.c_cflag &= ~CRTSCTS; 

#if defined(__linux__)
    speed_t speed;
    switch(baudRate) {
        case 300: speed = B300; break;
        case 600: speed = B600; break;
        case 1200: speed = B1200; break;
        case 2400: speed = B2400; break;
        case 4800: speed = B4800; break;
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        // Non-standard rates are wrapped in #ifdef so the build does not break
        // on distributions where these macros are not available.
#ifdef B128000
        case 128000: speed = B128000; break;
#endif
#ifdef B256000
        case 256000: speed = B256000; break;
#endif
#ifdef B500000
        case 500000: speed = B500000; break;
#endif
        default:
            std::cerr << "ERROR [SerialDataSource]: Baud rate not supported on Linux: " << baudRate << std::endl;
            ::close(fd); fd = -1;
            return false;
    }
    
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    if(tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "ERROR [SerialDataSource]: Failed to set serial port parameters." << std::endl;
        ::close(fd); fd = -1;
        return false;
    }
#elif defined(__APPLE__)
    // On macOS the parameters are applied first, then the arbitrary speed is
    // set with IOSSIOSPEED (the Bxxxx constants do not reach 500000)
    if(tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "ERROR [SerialDataSource]: Failed to set serial port parameters." << std::endl;
        ::close(fd); fd = -1;
        return false;
    }

    speed_t speed = baudRate;
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        std::cerr << "ERROR [SerialDataSource]: Baud rate not supported by Mac driver: " << baudRate << std::endl;
        ::close(fd); fd = -1;
        return false;
    }
#endif

    // Discard pending data (see PurgeComm on Windows)
    tcflush(fd, TCIOFLUSH);
    return true;
#endif
}

std::vector<uint8_t> SerialDataSource::readPacket()
{
    // Reads at most 1024 bytes per call; the vector is then shrunk to the bytes actually read
    std::vector<uint8_t> buffer(1024);
#if defined (_WIN32) || defined( _WIN64)
    if (!handle) {
        std::cerr << "ERROR [SerialDataSource]: Serial port not open." << std::endl;
        return {};
    }
    DWORD bytesRead;
    if(!ReadFile(handle, buffer.data(), buffer.size(), &bytesRead, nullptr)) {
        std::cerr << "ERROR [SerialDataSource]: Failed to read from serial port." << std::endl;
        return {};
    }
    buffer.resize(bytesRead);
    return buffer;
#endif
#if defined(__APPLE__) || defined(__linux__)
    if (fd < 0) {
        std::cerr << "ERROR [SerialDataSource]: Serial port not open." << std::endl;
        return {};
    }
    ssize_t bytesRead = ::read(fd, buffer.data(), buffer.size());
    if(bytesRead < 0) { 
        std::cerr << "ERROR [SerialDataSource]: Failed to read from serial port." << std::endl;
        return {};
        // Note: never reached (it comes after the return)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    buffer.resize(bytesRead);
    return buffer;
#endif
    return {};
}

void SerialDataSource::close()
{
#if defined (_WIN32) || defined( _WIN64)
    if (handle) {
        CloseHandle(handle);
        handle = nullptr;
    }
#endif
#if defined(__APPLE__) || defined(__linux__)
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
#endif
}

// List offered in the UI. On Linux some rates (128000, 256000, 500000) only
// work if the matching Bxxxx macros exist (see open()).
std::vector<int> SerialDataSource::supportedBaudRates() const {
#if defined(_WIN32) || defined(_WIN64)
    return {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 128000, 256000, 500000};
#elif defined(__APPLE__) || defined(__linux__)
    return {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 128000, 256000, 500000};
#else
    return {};
#endif
}