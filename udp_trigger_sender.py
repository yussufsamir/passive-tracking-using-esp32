import socket
import time

ESP32_IP = "192.168.8.138"
ESP32_PORT = 3333

PACKET_RATE_HZ = 5
MESSAGE = b"CSI_TRIGGER"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

interval = 1.0 / PACKET_RATE_HZ

print(f"Sending UDP trigger packets to {ESP32_IP}:{ESP32_PORT}")
print(f"Rate: {PACKET_RATE_HZ} packets/sec")

counter = 0

while True:
    payload = MESSAGE + counter.to_bytes(4, "little")
    sock.sendto(payload, (ESP32_IP, ESP32_PORT))
    counter += 1
    time.sleep(interval)