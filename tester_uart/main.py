# -*- coding: utf-8 -*-
"""ECU simulator over a serial port, with a small graphical interface (Tkinter).

Usage:
    pip install pyserial
    python tester_uart/main.py

It is used to test the telemetry software without the car: with two serial
ports connected to each other (e.g. a null-modem cable, two crossed USB-serial
adapters or a pair of virtual ports such as com0com/socat) start sending on one
port and connect the telemetry software to the other one, with the same baud
rate (default 500000).
"""
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from serial_worker import SerialSender

class TesterGUI:
    """Window with port and baud rate choice and START / STOP buttons."""
    def __init__(self, root):
        self.root = root
        self.root.title("Telemetry Spammer SAE")
        self.root.geometry("450x200") 
        self.root.resizable(False, False)

        # Selectable baud rates (500000 is the one used by the ECU)
        self.baud_rates =[9600, 19200, 38400, 115200, 128000, 256000, 500000, 1000000]

        # The sender reports errors and stops through callbacks
        self.sender = SerialSender()
        self.sender.on_error = self.handle_worker_error
        self.sender.on_stop = self.handle_worker_stop

        self.create_widgets()
        self.refresh_ports()

    def create_widgets(self):
        """Builds the window widgets."""
        pad = {'padx': 10, 'pady': 5}

        # Connection frame: port, refresh, baud rate
        frame_conn = ttk.LabelFrame(self.root, text="Connection")
        frame_conn.pack(fill="x", **pad)

        ttk.Label(frame_conn, text="Port:").pack(side="left", padx=(5, 2))
        self.combo_ports = ttk.Combobox(frame_conn, state="readonly", width=10)
        self.combo_ports.pack(side="left", padx=(0, 5))
        
        btn_refresh = ttk.Button(frame_conn, text="🔄", width=3, command=self.refresh_ports)
        btn_refresh.pack(side="left", padx=(0, 10))

        ttk.Label(frame_conn, text="Baud:").pack(side="left", padx=(5, 2))
        self.combo_baud = ttk.Combobox(frame_conn, values=self.baud_rates, state="readonly", width=12)
        self.combo_baud.pack(side="left", padx=(0, 5))
        self.combo_baud.set(500000)

        # START / STOP buttons
        frame_ctrl = ttk.Frame(self.root)
        frame_ctrl.pack(fill="both", expand=True, **pad)

        self.btn_start = ttk.Button(frame_ctrl, text="START SPAM", command=self.start_spam, style="G.TButton")
        self.btn_start.pack(side="left", expand=True, fill="both", padx=5, pady=10)

        self.btn_stop = ttk.Button(frame_ctrl, text="STOP", command=self.stop_spam, state="disabled", style="R.TButton")
        self.btn_stop.pack(side="right", expand=True, fill="both", padx=5, pady=10)

        # Status bar
        self.lbl_status = ttk.Label(self.root, text="Ready", relief="sunken", anchor="w")
        self.lbl_status.pack(side="bottom", fill="x")

        # Button styles (green / red)
        self.style = ttk.Style()
        self.style.configure("G.TButton", foreground="green", font=('Helvetica', 10, 'bold'))
        self.style.configure("R.TButton", foreground="red", font=('Helvetica', 10, 'bold'))

    def refresh_ports(self):
        """Fetches and updates the list of available serial ports."""
        ports = self.sender.list_ports()
        self.combo_ports['values'] = ports
        if ports:
            self.combo_ports.current(0)
            self.set_status(f"{len(ports)} ports found.")
        else:
            self.combo_ports.set('')
            self.set_status("No serial ports found.")

    def start_spam(self):
        """Starts the transmission on the chosen port and baud rate."""
        port = self.combo_ports.get()
        if not port:
            messagebox.showwarning("Warning", "Please select a serial port.")
            return
        
        try:
            baud = int(self.combo_baud.get())
        except (ValueError, TypeError):
            messagebox.showwarning("Warning", "Please select a valid baud rate.")
            return

        if self.sender.start(port, baudrate=baud):
            self.btn_start.config(state="disabled")
            self.btn_stop.config(state="normal")
            self.combo_ports.config(state="disabled")
            self.combo_baud.config(state="disabled") 
            self.set_status(f"Spamming on {port} at {baud} baud...")
        else:
            self.set_status("Error opening port.")

    def stop_spam(self):
        """Requests the transmission to stop."""
        self.set_status("Stopping...")
        self.sender.stop()
        
    def handle_worker_stop(self):
        """Sender callback when the transmission ends (moves back to the Tk thread with after())."""
        self.root.after(0, self._reset_gui_state)

    def handle_worker_error(self, error_msg):
        """Sender callback on error: shows the message (on the Tk thread with after())."""
        print(f"Error callback: {error_msg}")
        self.root.after(0, lambda: self.set_status(f"Error: {error_msg}"))
        self.root.after(0, lambda: messagebox.showerror("Serial Error", error_msg))

    def _reset_gui_state(self):
        """Restores the widgets to their initial state."""
        self.btn_start.config(state="normal")
        self.btn_stop.config(state="disabled")
        self.combo_ports.config(state="readonly")
        self.combo_baud.config(state="readonly") 
        if "Error" not in self.lbl_status['text']:
            self.set_status("Stopped.")

    def set_status(self, text):
        """Writes a message in the status bar."""
        self.lbl_status.config(text=f" {text}")

    def on_close(self):
        """Window close: stops sending and destroys the GUI."""
        self.sender.stop()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = TesterGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()