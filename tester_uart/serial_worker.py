# -*- coding: utf-8 -*-
"""Thread sending simulated telemetry packets over a serial port, with the same cadence as the ECU.

Cadence: one A packet every 5 ms and, every 40 ticks (200 ms), one B packet
right after the A packet. Requires pyserial (`pip install pyserial`).
"""
import serial
import serial.tools.list_ports
import threading
import time
import sys
from data_gen import generate_packet_A, generate_packet_B

class SerialSender:
    """Handles port opening, the sending thread and closing.

    Optional callbacks (called from the sending thread, NOT from the GUI thread):
        on_error(message) -> opening or writing error
        on_stop()         -> sending finished
    """
    def __init__(self):
        self.serial_port = None
        self._is_running = False
        self.thread = None
        self.on_error = None 
        self.on_stop = None

    @staticmethod
    def list_ports():
        """Lists the available serial ports across different operating systems."""
        ports =[]
        if sys.platform.startswith('win'):
            # Windows: tries to open COM1..COM256 and keeps the ones that open
            for i in range(1, 257):
                port_name = f'COM{i}'
                try:
                    s = serial.Serial(port_name)
                    s.close()
                    ports.append(port_name)
                except (OSError, serial.SerialException):
                    pass
        else:
            # POSIX systems: list provided by pyserial
            available_ports = serial.tools.list_ports.comports()
            ports = [port.device for port in available_ports]
        return ports

    def start(self, port_name, baudrate=256000):
        """Opens the port and starts the sending thread. Returns True if started."""
        if self._is_running:
            return

        try:
            self.serial_port = serial.Serial(port_name, baudrate, timeout=0.1)
            self._is_running = True
            
            self.thread = threading.Thread(target=self._sending_loop, daemon=True)
            self.thread.start()
            return True
        except serial.SerialException as e:
            if self.on_error:
                self.on_error(str(e))
            return False

    def stop(self):
        """Stops the transmission gracefully and closes the port."""
        if not self._is_running:
            return
            
        self._is_running = False
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=1.0) 
        
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.close()
            except:
                pass
        
        if self.on_stop:
            self.on_stop()

    def _sending_loop(self):        
        """Thread body: sends the packets at 5 ms intervals, as precisely as possible."""
        interval = 0.005  # exactly 5 milliseconds
        next_time = time.perf_counter() + interval
        tick_counter = 0  # 1 tick = 5 ms (it becomes the packet timestamp)

        while self._is_running:
            try:
                # Packet A: always sent, every 5 ms
                data_A = generate_packet_A(tick_counter)
                self.serial_port.write(data_A)

                # Packet B: sent every 40 ticks (200 ms)
                if tick_counter % 40 == 0:
                    data_B = generate_packet_B(tick_counter)
                    self.serial_port.write(data_B)

                # Precise synchronisation to 5 ms intervals
                now = time.perf_counter()
                sleep_time = next_time - now
                
                # sleep only when there is enough margin, then busy-wait for maximum precision
                if sleep_time > 0.002:
                    time.sleep(sleep_time - 0.001)
                while time.perf_counter() < next_time:
                    pass

                next_time += interval
                tick_counter += 1

            except serial.SerialException as e:
                print(f"Sending error: {e}")
                self._is_running = False
                if self.on_error:
                    self.on_error(f"Connection lost: {str(e)}")
                break
            except Exception as e:
                print(f"Unexpected error: {e}")
                self._is_running = False
                if self.on_error:
                    self.on_error(f"Error: {str(e)}")
                break

        self.stop()