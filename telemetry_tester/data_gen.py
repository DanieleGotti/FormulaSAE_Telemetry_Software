# -*- coding: utf-8 -*-
import struct
import random

def calculate_crc16(data: bytes) -> int:
    """Implementazione esatta del CRC16-CCITT del C++ in Python"""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def generate_packet_A(timestamp_val: int) -> bytes:
    """Genera il pacchetto telemetria principale (5ms)"""
    
    # 1. Metadata (Come richiesto: 0x00 e 0xCC)
    startByte1 = 0xCC
    startByte2 = 0x11
    timestamp = timestamp_val

    # 2. Generazione dati casuali
    fl_v = random.uniform(0.0, 120.0)
    fr_v = random.uniform(0.0, 120.0)
    rl_v = random.uniform(0.0, 120.0)
    rr_v = random.uniform(0.0, 120.0)
    
    fl_s = random.uniform(-5.0, 5.0)
    fr_s = random.uniform(-5.0, 5.0)
    
    acc1 = random.randint(0, 4000)
    acc2 = random.randint(0, 4000)
    acc_map = random.uniform(0.0, 100.0)
    
    brk1 = random.randint(800, 2000)
    brk2 = random.randint(800, 2000)
    steer = random.uniform(-180.0, 180.0)
    
    sdc = random.choice([0, 1])
    r2d = random.choice([0, 1])
    ecu_res = random.choice([0, 1])
    ts_on = random.choice([0, 1])
    
    emma_c = random.uniform(0.0, 250.0)
    emma_v = random.uniform(450.0, 600.0)
    emma_yaw = random.uniform(-1.0, 1.0)
    emma_error = random.choice([0, 1, 2]) # uint16_t
    
    mean_vel = (fl_v + fr_v) / 2
    real_yaw = random.uniform(-50.0, 50.0)
    tot_trq = random.uniform(0.0, 200.0)
    trq_tv_l = random.uniform(-10.0, 10.0)
    trq_tv_r = random.uniform(-10.0, 10.0)
    slip_l = random.uniform(0.0, 0.2)
    slip_r = random.uniform(0.0, 0.2)
    trq_red_l = random.uniform(0.0, 5.0)
    trq_red_r = random.uniform(0.0, 5.0)
    fin_trq_l = random.uniform(0.0, 100.0)
    fin_trq_r = random.uniform(0.0, 100.0)
    
    inv_L_tc = random.randint(-500, 500)
    inv_L_mc = random.randint(-100, 100)
    inv_L_tm = random.randint(20, 120)
    inv_R_tc = random.randint(-500, 500)
    inv_R_mc = random.randint(-100, 100)
    inv_R_tm = random.randint(20, 120)
    
    mot_L_tmp = random.uniform(20.0, 120.0)
    mot_R_tmp = random.uniform(20.0, 120.0)
    cool_L_tmp = random.uniform(20.0, 90.0)
    cool_R_tmp = random.uniform(20.0, 90.0)
    
    l_inv_fsm = random.randint(0, 8)
    r_inv_fsm = random.randint(0, 8)
    ts_fsm = random.randint(0, 5)
    ecu_mode = random.randint(0, 3)

    # 3. Formato Struct Payload A
    fmt = '< BB I 4f 2f 2H f 2H f 4B 3f H 11f 6h 4f 4B'
    
    payload = struct.pack(fmt,
        startByte1, startByte2, timestamp,
        fl_v, fr_v, rl_v, rr_v, fl_s, fr_s,
        acc1, acc2, acc_map, brk1, brk2, steer,
        sdc, r2d, ecu_res, ts_on,
        emma_c, emma_v, emma_yaw, emma_error,
        mean_vel, real_yaw, tot_trq, trq_tv_l, trq_tv_r, slip_l, slip_r, trq_red_l, trq_red_r, fin_trq_l, fin_trq_r,
        inv_L_tc, inv_L_mc, inv_L_tm, inv_R_tc, inv_R_mc, inv_R_tm,
        mot_L_tmp, mot_R_tmp, cool_L_tmp, cool_R_tmp,
        l_inv_fsm, r_inv_fsm, ts_fsm, ecu_mode
    )

    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)

def generate_packet_B(timestamp_val: int) -> bytes:
    """Genera il pacchetto EMMA batteria (200ms)"""
    
    # 1. Metadata (Come richiesto: 0x00 e 0xEE)
    startByte1 = 0xEE
    startByte2 = 0x11
    timestamp = timestamp_val
    
    # 2. Generazione dati EMMA
    soc = random.randint(0, 100)
    # Genero array di 14 voltaggi (uint8_t) e 28 temperature (uint16_t)
    voltages =[random.randint(30, 42) for _ in range(14)] # Esempio: 3.0V - 4.2V mappato a int
    temps =[random.randint(200, 600) for _ in range(28)]  # Esempio: 20.0C - 60.0C mappato a int
    
    # 3. Formato Struct Payload B
    # < BB I B 14B 28H -> Little Endian, metadata + soc + 14 uint8 + 28 uint16
    fmt = '< BB I B 14B 28H'
    
    # *voltages e *temps "spacchettano" le liste inserendole nei rispettivi 14B e 28H
    payload = struct.pack(fmt, startByte1, startByte2, timestamp, soc, *voltages, *temps)
    
    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)