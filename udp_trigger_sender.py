import socket
import time

# Add all ESP32 receiver IPs here
ESP32_TARGETS = [
    ("192.168.8.138", 3333),  # rx1
    ("192.168.8.2", 3333),    # rx2
     ("192.168.8.88", 3333), # rx3
]

PACKET_RATE_HZ = 3  # Start with 50, then try 100 / 200
MESSAGE = b"CSI_TRIGGER"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

interval = 1.0 / PACKET_RATE_HZ
counter = 0

print("Sending UDP trigger packets to ESP32 receivers...")
print(f"Packet rate per ESP: {PACKET_RATE_HZ} packets/sec")

while True:
    payload = MESSAGE + counter.to_bytes(4, "little")

    for ip, port in ESP32_TARGETS:
        sock.sendto(payload, (ip, port))

    counter += 1
    time.sleep(interval)