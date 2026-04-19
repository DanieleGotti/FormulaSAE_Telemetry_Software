#include <iostream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <filesystem>
#include <Telemetry/data_acquisition/SerialDataSource.hpp>

#if defined (_WIN32) || defined( _WIN64)
    #include <Windows.h>
#endif
#if defined(__APPLE__)
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>
    #include <stdio.h>
    #include <regex>
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
#if defined (__linux__)
	namespace fs = std::filesystem;
    const std::string DEV_PATH = "/dev/serial/by-id";
    m_availablePorts.clear();
    try
    {
        fs::path p(DEV_PATH);
        if (!fs::exists(DEV_PATH)) return m_availablePorts;
        for (fs::directory_entry de: fs::directory_iterator(p))
        {
            if (fs::is_symlink(de.symlink_status()))
            {
                fs::path symlink_points_at = fs::read_symlink(de);
                m_availablePorts.push_back(std::string("/dev/")+symlink_points_at.filename().c_str());
            }
        }
    }
    catch (const fs::filesystem_error &ex) {}
#endif
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
#if defined (_WIN32) || defined( _WIN64)
    handle = CreateFileA(resource.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(handle == INVALID_HANDLE_VALUE) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile aprire la porta seriale: " << resource.c_str() << "." << std::endl;
        handle = nullptr;
        return false;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if(!GetCommState(handle, &dcbSerialParams)) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile ottenere i parametri seriali correnti." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

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
            std::cerr << "ERRORE [SerialDataSource]: Baud rate non supportato: " << baudRate << "." << std::endl;
            CloseHandle(handle);
            handle = nullptr;
            return false;
    }

    dcbSerialParams.BaudRate = speed;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if(!SetCommState(handle, &dcbSerialParams)) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile impostare i parametri della porta seriale." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = MAXDWORD; // Ritorna immediatamente
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if(!SetCommTimeouts(handle, &timeouts)) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile impostare i timeout della porta seriale." << std::endl;
        CloseHandle(handle);
        handle = nullptr;
        return false;
    }

    return true;
#endif
#if defined(__APPLE__)
    fd = ::open(resource.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if(fd < 0) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile aprire la porta seriale: " << resource << "." << std::endl;
        return false;
    }

    struct termios tty{};
    if(tcgetattr(fd, &tty) != 0) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile ottenere i parametri della porta seriale." << std::endl;
        ::close(fd);
        fd = -1;
        return false;
    }

    // Map baud rate
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
        case 128000: speed = B128000; break;
        case 256000: speed = B256000; break;
        case 500000: speed = B500000; break;
        default:
            std::cerr << "ERRORE [SerialDataSource]: Baud rate non supportato: " << baudRate << "." << std::endl;
            ::close(fd);
            fd = -1;
            return false;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo, no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read blocks until at least 1 char is available
    tty.c_cc[VTIME] = 0;            // 0.1 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls, enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CRTSCTS; // no hardware flow control

    if(tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile impostare i parametri della porta seriale." << std::endl;
        ::close(fd);
        fd = -1;
        return false;
    }

    return true;
#endif
}

std::vector<uint8_t> SerialDataSource::readPacket()
{
    std::vector<uint8_t> buffer(1024);
#if defined (_WIN32) || defined( _WIN64)
    if (!handle) {
        std::cerr << "ERRORE [SerialDataSource]: Porta seriale non aperta." << std::endl;
        return {};
    }
    DWORD bytesRead;
    if(!ReadFile(handle, buffer.data(), buffer.size(), &bytesRead, nullptr)) {
        std::cerr << "ERRORE [SerialDataSource]: Impossibile leggere dalla porta seriale." << std::endl;
        return {};
    }
    buffer.resize(bytesRead);
    return buffer;
#endif
#if defined(__APPLE__)
    if (fd < 0) {
        std::cerr << "ERRORE [SerialDataSource]: Porta seriale non aperta." << std::endl;
        return {};
    }
    ssize_t bytesRead = ::read(fd, buffer.data(), buffer.size());
    if(bytesRead < 0) { 
        std::cerr << "ERRORE [SerialDataSource]: Impossibile leggere dalla porta seriale." << std::endl;
        return {};
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
#if defined(__APPLE__)
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
#endif
}

std::vector<int> SerialDataSource::supportedBaudRates() const {
#if defined(_WIN32) || defined(_WIN64)
    return {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 128000, 256000, 500000};
#elif defined(__APPLE__)
    return {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 128000, 256000, 500000};
#else
    return {};
#endif
}