# -*- coding: utf-8 -*-
"""
Created on Sat Oct 11 22:29:50 2025

@author: Daniele Gotti
"""

import serial
import serial.tools.list_ports
import threading
import time
import random
import sys 
from data_gen import generate_packet

class SerialSender:
    def __init__(self):
        self.serial_port = None
        self._is_running = False
        self.thread = None
        self.on_error = None 
        self.on_stop = None

    @staticmethod
    def list_ports():
        """
        Scansiona e restituisce una lista di porte seriali disponibili.
        Su Windows, utilizza un metodo di "brute-force" per trovare anche
        le porte virtuali che list_ports.comports() potrebbe non vedere.
        """
        ports = []
        # Metodo standard per Windows
        if sys.platform.startswith('win'):
            print("Scanning for COM ports on Windows...")
            for i in range(1, 257):
                port_name = f'COM{i}'
                try:
                    s = serial.Serial(port_name)
                    s.close()
                    ports.append(port_name)
                except (OSError, serial.SerialException):
                    pass
        else:
            # Metodo standard per Linux, macOS, etc.
            print("Scanning for ports on non-Windows OS...")
            available_ports = serial.tools.list_ports.comports()
            ports = [port.device for port in available_ports]

        return ports

    def start(self, port_name, baudrate=115200):
        """Apre la porta e avvia il thread di invio."""
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
        """Segnala al thread di fermarsi e chiude la porta."""
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
        """Il ciclo che gira nel thread secondario."""
        print("Thread invio avviato.")
        while self._is_running:
            try:
                data = generate_packet()
                self.serial_port.write(data)
                time.sleep(random.uniform(0.001, 0.01)) 

            except serial.SerialException as e:
                print(f"Errore durante l'invio: {e}")
                self._is_running = False 
                if self.on_error:
                    self.root.after(0, self.on_error, f"Connessione persa: {str(e)}")
                break 
            except Exception as e:
                print(f"Errore imprevisto: {e}")
                self._is_running = False
                if self.on_error:
                     self.root.after(0, self.on_error, f"Errore: {str(e)}")
                break

        self.stop()
        print("Thread invio terminato.")