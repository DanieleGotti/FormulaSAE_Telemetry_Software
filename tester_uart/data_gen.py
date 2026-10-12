# -*- coding: utf-8 -*-
"""Generator of random telemetry packets, identical in format to the ECU ones.

Used by serial_worker.py (serial simulator). When run directly it also writes
a 10 s .bin file next to the script (like tester_bin_sd/main.py).

Format (see include/Telemetry/data_acquisition/TelemetrySchema.hpp):
    [start1][start2][timestamp u32][fields...][CRC16 u16], all little-endian.

Packets follow the CURRENT layout (fieldsA_current() / fieldsB_current() in
TelemetrySchema.cpp). Every time TelemetrySchema.cpp changes this file must be
updated too (same order and same types of the fields), together with the copy
in tester_bin_sd/main.py.
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
    """Generates an A packet (0xCC 0x11, fast data every 5 ms) with random values.
    
    timestamp_val: counter in 5 ms ticks.
    Returns the packet bytes, CRC included.
    """

    # 1. Header: start bytes 0xCC 0x11 and timestamp
    startByte1 = 0xCC
    startByte2 = 0x11
    timestamp = timestamp_val

    # 2. Random values, in the order of the fields of fieldsA_current() in TelemetrySchema.cpp
    # Wheel speeds (m/s, the software converts them to km/h)
    fl_v = random.uniform(0.0, 100.0)
    fr_v = random.uniform(0.0, 100.0)
    rl_v = random.uniform(0.0, 100.0)
    rr_v = random.uniform(0.0, 100.0)
    
    # Front suspensions
    fl_s = random.uniform(-5.0, 5.0)
    fr_s = random.uniform(-5.0, 5.0)
    
    # Accelerator (2 raw sensors + mapped value)
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
    
    # EMMA: current, voltage, yaw, one random error bit
    emma_c = random.uniform(0.0, 250.0)
    emma_v = random.uniform(450.0, 600.0)
    emma_yaw = random.uniform(-1.0, 1.0)
    emma_error = random.choice([0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0800, 0x8000])

    # Vehicle dynamics and torque targets (11 floats)
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
    
    # Inverters: int16 transmitted x10 (e.g. 853 = 85.3)
    inv_L_tc = random.randint(-500, 500)
    inv_L_mc = random.randint(-100, 100)
    inv_L_tm = random.randint(200, 1200)
    inv_R_tc = random.randint(-500, 500)
    inv_R_mc = random.randint(-100, 100)
    inv_R_tm = random.randint(200, 1200)
    
    # Coolant temperatures
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

    # 3. struct format: '<' little-endian, B=u8, H=u16, h=i16, I=u32, f=float32.
    #    It must match the packet layout in TelemetrySchema.cpp field by field.
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

    # 4. CRC computed over all the preceding bytes, appended little-endian
    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)

def generate_packet_B(timestamp_val: int) -> bytes:
    """Generates a B packet (0xEE 0x11, EMMA battery every 200 ms) with random values.
    
    timestamp_val: counter in 5 ms ticks.
    Returns the packet bytes, CRC included.
    """

    # 1. Header: start bytes 0xEE 0x11 and timestamp
    startByte1 = 0xEE
    startByte2 = 0x11
    timestamp = timestamp_val
    
    # 2. EMMA data: state of charge
    soc = random.randint(0, 100)
    
    # 14 voltages (uint8) and 28 temperatures (uint16, two per module, interleaved)
    # Voltage: raw = V*100 - 200, so 100 = 3.0 V and 220 = 4.2 V
    voltages = [random.randint(100, 220) for _ in range(14)]
    temps = [random.randint(200, 600) for _ in range(28)]  # 20.0°C to 60.0°C (x10)

    # Bitmask of the 14 modules: bit set = module online. Occasionally drop one offline
    online_modules = 0x3FFF
    if random.random() < 0.1:
        online_modules &= ~(1 << random.randint(0, 13)) & 0x3FFF

    # 3. Format: start + timestamp + soc + 14 uint8 + 28 uint16 + online bitmask
    fmt = '< BB I B 14B 28H H'

    # *voltages and *temps "unpack" the lists into the matching 14B and 28H
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
            
            # Packet B: every 200 ms, i.e. every 40 ticks (right after packet A, like the ECU)
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