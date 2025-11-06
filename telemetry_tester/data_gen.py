# -*- coding: utf-8 -*-
"""
Created on Sat Oct 11 22:30:16 2025

@author: Daniele Gotti
"""

import random

# Dati principali (ACC, BRK, STEER)
EXPECTED_LABELS = ['ACC1A', 'ACC2A', 'ACC1B', 'ACC2B', 
                    'BRK1', 'BRK2', 'STEER',
                    'SOSPASX', 'SOSPADX', 'SOSPPSX', 'SOSPPDX'
                   ]

# Messaggi speciali/LED
STATUS_LABELS = ['SDC_INPUT', 'RESET_BUTTON', 'TS_ON_BUTTON', 'R2D_BUTTON']

# Messaggi di stato degli Inverter
INVERTER_LABELS = ['LEFT_INVERTER_FSM', 'RIGHT_INVERTER_FSM']

def generate_packet():
    """
    Genera un pacchetto di dati casuale in uno dei formati attesi dal ricevitore.
    Formato: [ETICHETTA] [VALORE]\n
    Senza timestamp e con lo spazio come separatore.
    """
    message_type = random.choices(
        population=['main_data', 'status', 'state', 'malformed'], 
        weights=[80, 15, 5, 0.5], 
        k=1
    )[0]

    packet = ""

    if message_type == 'main_data':
        label = random.choice(EXPECTED_LABELS)
        
        if label.startswith('ACC'):
            # ACC1A, ACC2A sono int
            if label.endswith('A'):
                value = random.randint(0, 4000)
            else:
                # ACC1B, ACC2B sono float
                value = round(random.uniform(0.0, 4000.0), 1)
        elif label.startswith('BRK'):
            # BRK1, BRK2 sono float
            value = round(random.uniform(0.0, 100.0), 1)
        elif label == 'STEER':
            # STEER è float
            value = round(random.uniform(-180.0, 180.0), 1)
        elif label.startswith('SOS'):
            # Sospensioni sono float
            value = round(random.uniform(-4.0, 4.0), 2)

        packet = f"{label} {value}\n"

    # Formato messaggi di stato/LED: es. "SDC_INPUT 1"
    elif message_type == 'status':
        label = random.choice(STATUS_LABELS)
        value = random.choice([0, 1])
        packet = f"{label} {value}\n"

    # Formato stato Inverter: es. "STATE RIGHT_INVERTER_FSM 5"
    elif message_type == 'state':
        inverter_label = random.choice(INVERTER_LABELS)
        state_value = random.randint(0, 8)
        packet = f"STATE {inverter_label} {state_value}\n"

    # Formati malformati per testare la robustezza del parser
    elif message_type == 'malformed':
        malformed_options = [
            # Valore mancante
            f"{random.choice(EXPECTED_LABELS)}\n",
            # Valore mancante per lo stato
            f"STATE {random.choice(INVERTER_LABELS)}\n",
            # Etichetta sconosciuta
            "UNKNOWN_SENSOR 42\n",
            # Tipo di dato sbagliato (stringa invece di numero)
            f"{random.choice(EXPECTED_LABELS)} not_a_number\n",
            # Troppe parti nel messaggio
            f"{random.choice(EXPECTED_LABELS)} 123 extra_data\n",
            f"STATE {random.choice(INVERTER_LABELS)} 5 extra_part\n",
            # Parola chiave sbagliata
            f"STATUS {random.choice(INVERTER_LABELS)} 3\n",
            # Riga vuota
            "\n",
            # Riga con solo spazi
            "   \n"
        ]
        packet = random.choice(malformed_options)    
        
    return packet.encode('utf-8') # Restituisce i bytes pronti per la seriale