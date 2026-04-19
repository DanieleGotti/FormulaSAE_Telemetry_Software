# -*- coding: utf-8 -*-
import serial
import serial.tools.list_ports
import threading
import time
import sys
from data_gen import generate_packet_A, generate_packet_B

class SerialSender:
    def __init__(self):
        self.serial_port = None
        self._is_running = False
        self.thread = None
        self.on_error = None 
        self.on_stop = None

    @staticmethod
    def list_ports():
        ports = []
        if sys.platform.startswith('win'):
            for i in range(1, 257):
                port_name = f'COM{i}'
                try:
                    s = serial.Serial(port_name)
                    s.close()
                    ports.append(port_name)
                except (OSError, serial.SerialException):
                    pass
        else:
            available_ports = serial.tools.list_ports.comports()
            ports = [port.device for port in available_ports]
        return ports

    def start(self, port_name, baudrate=256000):
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
        interval = 0.005  # 5 millisecondi esatti
        next_time = time.perf_counter() + interval
        tick_counter = 0  # ogni tick = 5ms

        while self._is_running:
            try:
                # Pacchetto A — sempre, ogni 5ms
                data_A = generate_packet_A(tick_counter)
                self.serial_port.write(data_A)

                # Pacchetto B — sempre, ogni 40 tick (200ms)
                if tick_counter % 40 == 0:
                    data_B = generate_packet_B(tick_counter)
                    self.serial_port.write(data_B)

                # Sincronizzazione precisa a 5ms
                now = time.perf_counter()
                sleep_time = next_time - now
                if sleep_time > 0.002:
                    time.sleep(sleep_time - 0.001)
                while time.perf_counter() < next_time:
                    pass

                next_time += interval
                tick_counter += 1

            except serial.SerialException as e:
                print(f"Errore durante l'invio: {e}")
                self._is_running = False
                if self.on_error:
                    self.on_error(f"Connessione persa: {str(e)}")
                break
            except Exception as e:
                print(f"Errore imprevisto: {e}")
                self._is_running = False
                if self.on_error:
                    self.on_error(f"Errore: {str(e)}")
                break

        self.stop()