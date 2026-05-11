# -*- coding: utf-8 -*-
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from serial_worker import SerialSender

class TesterGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Telemetry Spammer SAE")
        self.root.geometry("450x200") 
        self.root.resizable(False, False)

        self.baud_rates =[9600, 19200, 38400, 115200, 128000, 256000, 500000, 1000000]

        self.sender = SerialSender()
        self.sender.on_error = self.handle_worker_error
        self.sender.on_stop = self.handle_worker_stop

        self.create_widgets()
        self.refresh_ports()

    def create_widgets(self):
        pad = {'padx': 10, 'pady': 5}

        # Connection Frame
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

        # Control Buttons Frame
        frame_ctrl = ttk.Frame(self.root)
        frame_ctrl.pack(fill="both", expand=True, **pad)

        self.btn_start = ttk.Button(frame_ctrl, text="START SPAM", command=self.start_spam, style="G.TButton")
        self.btn_start.pack(side="left", expand=True, fill="both", padx=5, pady=10)

        self.btn_stop = ttk.Button(frame_ctrl, text="STOP", command=self.stop_spam, state="disabled", style="R.TButton")
        self.btn_stop.pack(side="right", expand=True, fill="both", padx=5, pady=10)

        # Status Label
        self.lbl_status = ttk.Label(self.root, text="Ready", relief="sunken", anchor="w")
        self.lbl_status.pack(side="bottom", fill="x")

        # Custom Styles for Buttons
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
        """Initiates the data transmission."""
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
        """Triggers the stopping of transmission."""
        self.set_status("Stopping...")
        self.sender.stop()
        
    def handle_worker_stop(self):
        """Callback executed when the serial worker stops cleanly."""
        self.root.after(0, self._reset_gui_state)

    def handle_worker_error(self, error_msg):
        """Callback executed when the serial worker encounters an error."""
        print(f"Error callback: {error_msg}")
        self.root.after(0, lambda: self.set_status(f"Error: {error_msg}"))
        self.root.after(0, lambda: messagebox.showerror("Serial Error", error_msg))

    def _reset_gui_state(self):
        """Restores the GUI elements to their initial state."""
        self.btn_start.config(state="normal")
        self.btn_stop.config(state="disabled")
        self.combo_ports.config(state="readonly")
        self.combo_baud.config(state="readonly") 
        if "Error" not in self.lbl_status['text']:
            self.set_status("Stopped.")

    def set_status(self, text):
        self.lbl_status.config(text=f" {text}")

    def on_close(self):
        """Handles the application exit event."""
        self.sender.stop()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = TesterGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()