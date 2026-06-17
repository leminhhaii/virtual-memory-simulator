import random
import os

os.makedirs('tests/traces', exist_ok=True)
random.seed(42)

# Generate Sequential Scan (Loop over 10 pages)
with open('tests/traces/bench_seq.txt', 'w') as f:
    for i in range(100):
        vpn = i % 10
        offset = 0
        addr = (vpn << 12) + offset
        f.write(f"R 0x{addr:04X}\n")

# Generate High Locality (80% chance to stay on same page)
with open('tests/traces/bench_loc.txt', 'w') as f:
    vpn = 0
    for i in range(100):
        if random.random() > 0.8:
            vpn = random.randint(0, 9)
        offset = random.randint(0, 4095)
        addr = (vpn << 12) + offset
        f.write(f"R 0x{addr:04X}\n")

# Generate Mixed Reads/Writes
with open('tests/traces/bench_mix.txt', 'w') as f:
    for i in range(100):
        vpn = random.randint(0, 9)
        offset = random.randint(0, 4095)
        addr = (vpn << 12) + offset
        op = 'W' if random.random() > 0.7 else 'R'
        f.write(f"{op} 0x{addr:04X}\n")

# Generate Thrashing (Loops over 8 pages rapidly)
with open('tests/traces/bench_thr.txt', 'w') as f:
    for i in range(100):
        vpn = i % 8
        offset = 0
        addr = (vpn << 12) + offset
        f.write(f"R 0x{addr:04X}\n")

print("Generated benchmark traces in tests/traces/")
