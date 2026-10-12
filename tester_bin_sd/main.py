# -*- coding: utf-8 -*-
"""Generates a test .bin file, identical to the one the ECU writes to the SD card.

Usage:
    python tester_bin_sd/main.py

Creates `tester_bin.bin` (10 s of random data) in the script folder: it can
then be opened in the software with "Load from file". The format is the same
binary stream sent over serial (A packets every 5 ms, B packets every 200 ms).

The functions are copies of the ones in tester_uart/data_gen.py: if the packet
format changes BOTH files must be updated. Packets follow the current layout
defined in TelemetrySchema.cpp.
"""
import struct
import random
import os

def calculate_crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF): identical to Calculate_CRC16() in C++."""
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
    """Generates an A packet (0xCC 0x11, fast data every 5 ms). See tester_uart/data_gen.py for the field details."""
    
    # Header
    startByte1 = 0xCC
    startByte2 = 0x11
    timestamp = timestamp_val

    # Wheel speeds
    fl_v = random.uniform(0.0, 100.0)
    fr_v = random.uniform(0.0, 100.0)
    rl_v = random.uniform(0.0, 100.0)
    rr_v = random.uniform(0.0, 100.0)
    
    # Suspensions
    fl_s = random.uniform(-5.0, 5.0)
    fr_s = random.uniform(-5.0, 5.0)
    
    # Accelerator
    acc1 = random.randint(0, 100)
    acc2 = random.randint(0, 100)
    acc_map = random.uniform(0.0, 100.0)
    
    # Brake and steering
    brk1 = random.randint(0, 100)
    brk2 = random.randint(0, 100)
    steer = random.uniform(-180.0, 180.0)
    
    # Digital inputs
    sdc = random.choice([0, 1])
    r2d = random.choice([0, 1])
    ecu_res = random.choice([0, 1])
    ts_on = random.choice([0, 1])
    
    # EMMA
    emma_c = random.uniform(0.0, 250.0)
    emma_v = random.uniform(450.0, 600.0)
    emma_yaw = random.uniform(-1.0, 1.0)
    emma_error = random.choice([0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0800, 0x8000])

    # Vehicle dynamics and torque targets
    mean_vel = random.uniform(0.0, 120.0)
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
    
    # Inverters (int16 x10)
    inv_L_tc = random.randint(-500, 500)
    inv_L_mc = random.randint(-100, 100)
    inv_L_tm = random.randint(200, 1200)
    inv_R_tc = random.randint(-500, 500)
    inv_R_mc = random.randint(-100, 100)
    inv_R_tm = random.randint(200, 1200)
    
    # Cooling
    cool_L_tmp = random.uniform(20.0, 90.0)
    cool_R_tmp = random.uniform(20.0, 90.0)
    
    # State machines and ECU mode
    l_inv_fsm = random.randint(0, 8)
    r_inv_fsm = random.randint(0, 8)
    ts_fsm = random.randint(0, 9)
    ecu_mode = random.choice([0x01, 0x02, 0x04])

    # IMD insulation resistance in kOhm: mostly healthy, occasionally near/below the FSAE limit
    if random.random() < 0.1:
        imd_insulation = random.randint(0, 300)
    else:
        imd_insulation = random.randint(500, 5000)

    # AMK diagnostic per inverter (0 = none): almost always 0, sometimes a warning
    # (1073, 2350) or an error (2310, 2346, 2347). See AmkDiagnostics.hpp
    amk_codes = [1073, 2350, 2310, 2346, 2347]
    inv_L_diag = random.choice(amk_codes) if random.random() < 0.05 else 0
    inv_R_diag = random.choice(amk_codes) if random.random() < 0.05 else 0

    # Little-endian format, field by field like the layout in TelemetrySchema.cpp
    fmt = '< BB I 4f 2f 2H f 2H f 4B 3f H 11f 6h 2f 4B H 2H'

    payload = struct.pack(fmt,
        startByte1, startByte2, timestamp,
        fl_v, fr_v, rl_v, rr_v, fl_s, fr_s,
        acc1, acc2, acc_map, brk1, brk2, steer,
        sdc, r2d, ecu_res, ts_on,
        emma_c, emma_v, emma_yaw, emma_error,
        mean_vel, real_yaw, tot_trq, trq_tv_l, trq_tv_r, slip_l, slip_r, trq_red_l, trq_red_r, fin_trq_l, fin_trq_r,
        inv_L_tc, inv_L_mc, inv_L_tm, inv_R_tc, inv_R_mc, inv_R_tm,
        cool_L_tmp, cool_R_tmp,
        l_inv_fsm, r_inv_fsm, ts_fsm, ecu_mode,
        imd_insulation,
        inv_L_diag, inv_R_diag
    )

    # CRC over all the preceding bytes
    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)

def generate_packet_B(timestamp_val: int) -> bytes:
    """Generates a B packet (0xEE 0x11, EMMA battery every 200 ms)."""
    
    startByte1 = 0xEE
    startByte2 = 0x11
    timestamp = timestamp_val
    
    # State of charge, 14 voltages (raw = V*100 - 200), 28 temperatures (x10)
    soc = random.randint(0, 100)
    voltages =[random.randint(100, 220) for _ in range(14)]
    temps =[random.randint(200, 600) for _ in range(28)]

    # Online module bitmask (occasionally one module offline)
    online_modules = 0x3FFF
    if random.random() < 0.1:
        online_modules &= ~(1 << random.randint(0, 13)) & 0x3FFF

    fmt = '< BB I B 14B 28H H'

    payload = struct.pack(fmt, startByte1, startByte2, timestamp, soc, *voltages, *temps, online_modules)
    
    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)

def generate_telemetry_file(filename="tester_bin.bin", duration_sec=10):
    """Generates exactly `duration_sec` seconds of data and saves it to a .bin next to the script."""
    
    interval_ms = 5
    total_ticks = int((duration_sec * 1000) / interval_ms)
    
    count_A = 0
    count_B = 0

    # Folder containing this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # The file is saved in the same folder
    file_path = os.path.join(script_dir, filename)

    print(f"Generating file '{file_path}'...")
    print(f"Simulated duration: {duration_sec} seconds.")
    
    # Binary write mode ("wb")
    with open(file_path, "wb") as f:
        for tick_counter in range(total_ticks):
            
            # Packet A: every 5 ms, i.e. at every iteration
            packet_A = generate_packet_A(tick_counter)
            f.write(packet_A)
            count_A += 1
            
            # Packet B: every 200 ms, i.e. every 40 ticks
            if tick_counter % 40 == 0:
                packet_B = generate_packet_B(tick_counter)
                f.write(packet_B)
                count_B += 1
                
    # Final statistics
    print("\nGeneration completed successfully!")
    print("Statistics:")
    print(f" - Packet A (5ms) generated:  {count_A}")
    print(f" - Packet B (200ms) generated: {count_B}")
    print(f"Total packets written to file: {count_A + count_B}")

if __name__ == "__main__":
    # Direct execution: generates 10 s of data
    generate_telemetry_file("tester_bin.bin", 10)