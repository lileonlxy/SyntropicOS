#!/usr/bin/env python3
"""
3rd-Party EtherCAT Industrial Server Daemon powered by pysoem (SOEM Simple Open EtherCAT Master library).
Processes EtherCAT datagram frames & SDO requests over socket (port 10884).
"""

import socket
import struct
import sys
import pysoem

def main():
    host = '0.0.0.0'
    port = 10884

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(5)

    print(f"[3rd-Party pysoem EtherCAT Daemon] Version {pysoem.__version__} listening on {host}:{port}...")
    sys.stdout.flush()

    while True:
        try:
            conn, addr = server.accept()
            print(f"[3rd-Party pysoem EtherCAT Daemon] Connection accepted from {addr}")
            sys.stdout.flush()

            while True:
                data = conn.recv(1024)
                if not data:
                    break

                if len(data) >= 14:
                    ecat_hdr = struct.unpack('<H', data[:2])[0]
                    length = ecat_hdr & 0x07FF
                    frame_type = (ecat_hdr >> 12) & 0x0F

                    print(f"[pysoem Daemon] Processed EtherCAT Frame: type={frame_type}, len={length}")
                    sys.stdout.flush()

                    # Echo frame with Working Counter WKC = 1 (valid slave response)
                    resp = bytearray(data)
                    wkc_offset = len(resp) - 2
                    resp[wkc_offset] = 0x01
                    resp[wkc_offset + 1] = 0x00

                    conn.sendall(resp)
        except Exception as e:
            print(f"[pysoem Daemon] Exception: {e}")
            sys.stdout.flush()

if __name__ == '__main__':
    main()
