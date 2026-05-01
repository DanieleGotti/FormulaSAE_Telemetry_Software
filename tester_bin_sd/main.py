# -*- coding: utf-8 -*-
import struct
import random
import os

def calculate_crc16(data: bytes) -> int:
    """Exact implementation of C++ CRC16-CCITT in Python"""
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
    """Generates the main telemetry packet (5ms)"""
    
    startByte1 = 0xCC
    startByte2 = 0x11
    timestamp = timestamp_val

    fl_v = random.uniform(0.0, 100.0)
    fr_v = random.uniform(0.0, 100.0)
    rl_v = random.uniform(0.0, 100.0)
    rr_v = random.uniform(0.0, 100.0)
    
    fl_s = random.uniform(-5.0, 5.0)
    fr_s = random.uniform(-5.0, 5.0)
    
    acc1 = random.randint(0, 100)
    acc2 = random.randint(0, 100)
    acc_map = random.uniform(0.0, 100.0)
    
    brk1 = random.randint(0, 100)
    brk2 = random.randint(0, 100)
    steer = random.uniform(-180.0, 180.0)
    
    sdc = random.choice([0, 1])
    r2d = random.choice([0, 1])
    ecu_res = random.choice([0, 1])
    ts_on = random.choice([0, 1])
    
    emma_c = random.uniform(0.0, 250.0)
    emma_v = random.uniform(450.0, 600.0)
    emma_yaw = random.uniform(-1.0, 1.0)
    emma_error = random.choice([0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x8000]) 

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
    
    inv_L_tc = random.randint(-500, 500)
    inv_L_mc = random.randint(-100, 100)
    inv_L_tm = random.randint(20, 120)
    inv_R_tc = random.randint(-500, 500)
    inv_R_mc = random.randint(-100, 100)
    inv_R_tm = random.randint(20, 120)
    
    cool_L_tmp = random.uniform(20.0, 90.0)
    cool_R_tmp = random.uniform(20.0, 90.0)
    mot_R_tmp = random.uniform(20.0, 120.0)
    mot_L_tmp = random.uniform(20.0, 120.0)
    
    l_inv_fsm = random.randint(0, 8)
    r_inv_fsm = random.randint(0, 8)
    ts_fsm = random.randint(0, 9)
    ecu_mode = random.choice([0x01, 0x02, 0x04])

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
    """Generates the EMMA battery packet (200ms)"""
    
    startByte1 = 0xEE
    startByte2 = 0x11
    timestamp = timestamp_val
    
    soc = random.randint(0, 100)
    voltages =[random.randint(30, 42) for _ in range(14)] 
    temps =[random.randint(200, 600) for _ in range(28)]  
    
    fmt = '< BB I B 14B 28H'
    
    payload = struct.pack(fmt, startByte1, startByte2, timestamp, soc, *voltages, *temps)
    
    crc = calculate_crc16(payload)
    return payload + struct.pack('<H', crc)

def generate_telemetry_file(filename="tester_bin.bin", duration_sec=10):
    """Generates exactly `duration_sec` of data and saves it in a binary file at the script's location."""
    
    interval_ms = 5
    total_ticks = int((duration_sec * 1000) / interval_ms)
    
    count_A = 0
    count_B = 0

    # Get the exact directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Save the file directly in the same directory
    file_path = os.path.join(script_dir, filename)

    print(f"Generating file '{file_path}'...")
    print(f"Simulated duration: {duration_sec} seconds.")
    
    # Open the file in binary write mode ("wb")
    with open(file_path, "wb") as f:
        for tick_counter in range(total_ticks):
            
            # Packet A (generated every 5ms, so at each iteration)
            packet_A = generate_packet_A(tick_counter)
            f.write(packet_A)
            count_A += 1
            
            # Packet B (generated every 200ms, meaning every 40 ticks)
            if tick_counter % 40 == 0:
                packet_B = generate_packet_B(tick_counter)
                f.write(packet_B)
                count_B += 1
                
    # Print final statistics
    print("\nGeneration completed successfully!")
    print("Statistics:")
    print(f" - Packet A (5ms) generated:  {count_A}")
    print(f" - Packet B (200ms) generated: {count_B}")
    print(f"Total packets written to file: {count_A + count_B}")

if __name__ == "__main__":
    # Execute the generation
    generate_telemetry_file("tester_bin.bin", 10)