import time
import serial
import csv


# Adjust these:
SERIAL_PORT = 'COM9'
BAUD_RATE = 1000000
OUTPUT_CSV = './IMUDatasets/InnerRail_4_16G.csv'
DURATION_SECONDS = 15 * 60  # 1 second

ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

print(f"Logging data from {SERIAL_PORT}...")

start_time = time.time()
with open(OUTPUT_CSV, 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(['x', 'y', 'z'])  # Column headers

    while True:
        if time.time() - start_time > DURATION_SECONDS:
            print("Logging completed.")
            break
        
        line = ser.readline().decode('utf-8').strip()
        if line:
            try:
                parts = line.replace(':', '').split()
                x = float(parts[1])
                y = float(parts[3])
                z = float(parts[5])
                writer.writerow([x, y, z])
                print(f"Logged: {x}, {y}, {z}")
            except Exception as e:
                print(f"Skipping invalid line: {line} ({e})")