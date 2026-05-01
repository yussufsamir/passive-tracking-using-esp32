import socket
import struct
import csv
from datetime import datetime

UDP_IP = "0.0.0.0"
UDP_PORT = 5005
OUTPUT_FILE = "csi_data_formatted.csv"

# Must match ESP32 C struct:
# char device_id[8]
# uint32_t esp_timestamp
# int8_t rssi
# uint8_t channel
# uint16_t csi_len
HEADER_FORMAT = "<8sIbbH"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening on UDP port {UDP_PORT}...")
print(f"Saving to {OUTPUT_FILE}")

with open(OUTPUT_FILE, "a", newline="") as file:
    writer = csv.writer(file)

    writer.writerow(["esp_id", "timestamp", "rssi", "csi"])

    while True:
        data, addr = sock.recvfrom(4096)

        if len(data) < HEADER_SIZE:
            continue

        header = data[:HEADER_SIZE]
        csi_raw = data[HEADER_SIZE:]

        device_id_raw, esp_timestamp, rssi, channel, csi_len = struct.unpack(
            HEADER_FORMAT,
            header
        )

        esp_id = device_id_raw.decode(errors="ignore").strip("\x00")

        if len(csi_raw) != csi_len:
            continue

        csi_array = list(struct.unpack(f"{csi_len}b", csi_raw))

        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

        writer.writerow([
            esp_id,
            timestamp,
            rssi,
            str(csi_array)
        ])

        file.flush()

        print(esp_id, timestamp, rssi, f"CSI length: {csi_len}")