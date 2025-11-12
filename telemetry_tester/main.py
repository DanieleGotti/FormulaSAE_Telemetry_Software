# -*- coding: utf-8 -*-
"""
Created on Sat Oct 11 22:27:31 2025

@author: Daniele Gotti
"""

import tkinter as tk
from tkinter import ttk
from tkinter import messagebox
from serial_worker import SerialSender

class TesterGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Telemetry Spammer")
        self.root.geometry("450x200") 
        self.root.resizable(False, False)

        self.baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 115200, 128000, 256000]

        self.sender = SerialSender()
        self.sender.on_error = self.handle_worker_error
        self.sender.on_stop = self.handle_worker_stop

        self.create_widgets()
        self.refresh_ports()

    def create_widgets(self):
        pad = {'padx': 10, 'pady': 5}

        frame_conn = ttk.LabelFrame(self.root, text="Connessione")
        frame_conn.pack(fill="x", **pad)

        ttk.Label(frame_conn, text="Porta:").pack(side="left", padx=(5, 2))
        self.combo_ports = ttk.Combobox(frame_conn, state="readonly", width=10)
        self.combo_ports.pack(side="left", padx=(0, 5))
        
        btn_refresh = ttk.Button(frame_conn, text="🔄", width=3, command=self.refresh_ports)
        btn_refresh.pack(side="left", padx=(0, 10))

        ttk.Label(frame_conn, text="Baud:").pack(side="left", padx=(5, 2))
        self.combo_baud = ttk.Combobox(frame_conn, values=self.baud_rates, state="readonly", width=12)
        self.combo_baud.pack(side="left", padx=(0, 5))
        self.combo_baud.set(115200)

        frame_ctrl = ttk.Frame(self.root)
        frame_ctrl.pack(fill="both", expand=True, **pad)

        self.btn_start = ttk.Button(frame_ctrl, text="AVVIA SPAM", command=self.start_spam, style="G.TButton")
        self.btn_start.pack(side="left", expand=True, fill="both", padx=5, pady=10)

        self.btn_stop = ttk.Button(frame_ctrl, text="STOP", command=self.stop_spam, state="disabled", style="R.TButton")
        self.btn_stop.pack(side="right", expand=True, fill="both", padx=5, pady=10)

        self.lbl_status = ttk.Label(self.root, text="Pronto", relief="sunken", anchor="w")
        self.lbl_status.pack(side="bottom", fill="x")

        self.style = ttk.Style()
        self.style.configure("G.TButton", foreground="green", font=('Helvetica', 10, 'bold'))
        self.style.configure("R.TButton", foreground="red", font=('Helvetica', 10, 'bold'))

    def refresh_ports(self):
        ports = self.sender.list_ports()
        self.combo_ports['values'] = ports
        if ports:
            self.combo_ports.current(0)
            self.set_status(f"{len(ports)} porte trovate.")
        else:
            self.combo_ports.set('')
            self.set_status("Nessuna porta seriale trovata.")

    def start_spam(self):
        port = self.combo_ports.get()
        if not port:
            messagebox.showwarning("Attenzione", "Seleziona una porta seriale.")
            return
        
        try:
            baud = int(self.combo_baud.get())
        except (ValueError, TypeError):
            messagebox.showwarning("Attenzione", "Seleziona un baud rate valido.")
            return

        if self.sender.start(port, baudrate=baud):
            self.btn_start.config(state="disabled")
            self.btn_stop.config(state="normal")
            self.combo_ports.config(state="disabled")
            self.combo_baud.config(state="disabled") 
            self.set_status(f"Spamming su {port} a {baud} baud...")
        else:
            self.set_status("Errore apertura porta.")

    def stop_spam(self):
        self.set_status("Arresto in corso...")
        self.sender.stop()
        
    def handle_worker_stop(self):
        """Chiamato dal worker quando si ferma (manualmente o per errore)."""
        self.root.after(0, self._reset_gui_state)

    def handle_worker_error(self, error_msg):
        """Chiamato dal worker se c'è un'eccezione (es. porta virtuale chiusa)."""
        print(f"Callback errore: {error_msg}")
        self.root.after(0, lambda: self.set_status(f"Errore: {error_msg}"))
        self.root.after(0, lambda: messagebox.showerror("Errore Seriale", error_msg))

    def _reset_gui_state(self):
        """MODIFICATO: Riabilita anche il menu del baud rate."""
        self.btn_start.config(state="normal")
        self.btn_stop.config(state="disabled")
        self.combo_ports.config(state="readonly")
        self.combo_baud.config(state="readonly") 
        if "Errore" not in self.lbl_status['text']:
            self.set_status("Fermato.")

    def set_status(self, text):
        self.lbl_status.config(text=f" {text}")

    def on_close(self):
        self.sender.stop()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = TesterGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()