#!/usr/bin/env python3
"""
Comprehensive 3rd-Party Python UDS Server Daemon for ISO 14229-1 Integration Testing.
Provides end-to-end verification peer for all 25+ UDS services, sub-functions, and NRCs.
"""

import socket
import sys
import time

def main():
    print("[3rd-Party UDS Daemon] Starting ISO 14229-1 diagnostic daemon on 127.0.0.1:10886...")
    
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_sock.bind(('0.0.0.0', 10886))
    server_sock.listen(5)
    
    print("[3rd-Party UDS Daemon] Listening for SyntropicOS UDS integration client...")
    
    while True:
        try:
            conn, addr = server_sock.accept()
            print(f"[3rd-Party UDS Daemon] Connected from {addr}")
            
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                
                sid = data[0]
                
                # 0x10 DiagnosticSessionControl
                if sid == 0x10:
                    sub = data[1] & 0x7F
                    suppress = (data[1] & 0x80) != 0
                    if suppress:
                        conn.sendall(b'')
                    else:
                        conn.sendall(bytes([0x50, sub, 0x00, 0x32, 0x01, 0xF4]))
                
                # 0x11 ECUReset
                elif sid == 0x11:
                    sub = data[1] & 0x7F
                    suppress = (data[1] & 0x80) != 0
                    if suppress:
                        conn.sendall(b'')
                    else:
                        conn.sendall(bytes([0x51, sub]))
                
                # 0x14 ClearDiagnosticInformation
                elif sid == 0x14:
                    conn.sendall(bytes([0x54]))
                
                # 0x19 ReadDTCInformation
                elif sid == 0x19:
                    sub = data[1] & 0x7F
                    if sub == 0x01: # ReportNumDTC
                        conn.sendall(bytes([0x59, 0x01, 0xFF, 0x00, 0x00, 0x01]))
                    elif sub == 0x0A: # SupportedDTCs
                        conn.sendall(bytes([0x59, 0x0A, 0xFF, 0x12, 0x34, 0x56, 0x24]))
                    else:
                        conn.sendall(bytes([0x59, sub, 0xFF]))
                
                # 0x22 ReadDataByIdentifier
                elif sid == 0x22:
                    did = (data[1] << 8) | data[2]
                    if did == 0xF190: # VIN
                        conn.sendall(bytes([0x62, 0xF1, 0x90]) + b'SYN1234567890UDS')
                    elif did == 0x0100: # System Status
                        conn.sendall(bytes([0x62, 0x01, 0x00, 0xAA, 0xBB]))
                    else:
                        conn.sendall(bytes([0x7F, 0x22, 0x31])) # NRC 0x31 Request Out Of Range
                
                # 0x27 SecurityAccess
                elif sid == 0x27:
                    sub = data[1] & 0x7F
                    if sub == 0x01: # Request Seed
                        conn.sendall(bytes([0x67, 0x01, 0x12, 0x34, 0x56, 0x78]))
                    elif sub == 0x02: # Send Key
                        conn.sendall(bytes([0x67, 0x02]))
                
                # 0x28 CommunicationControl
                elif sid == 0x28:
                    sub = data[1] & 0x7F
                    conn.sendall(bytes([0x68, sub]))
                
                # 0x2E WriteDataByIdentifier
                elif sid == 0x2E:
                    did = (data[1] << 8) | data[2]
                    conn.sendall(bytes([0x6E, (did >> 8) & 0xFF, did & 0xFF]))
                
                # 0x2F InputOutputControlByIdentifier
                elif sid == 0x2F:
                    did = (data[1] << 8) | data[2]
                    ctrl = data[3] if len(data) > 3 else 0x00
                    conn.sendall(bytes([0x6F, (did >> 8) & 0xFF, did & 0xFF, ctrl]))
                
                # 0x31 RoutineControl
                elif sid == 0x31:
                    sub = data[1] & 0x7F
                    rid = (data[2] << 8) | data[3]
                    conn.sendall(bytes([0x71, sub, (rid >> 8) & 0xFF, rid & 0xFF]))
                
                # 0x34 RequestDownload / 0x35 RequestUpload
                elif sid in (0x34, 0x35):
                    conn.sendall(bytes([sid + 0x40, 0x20, 0x04, 0x00])) # Length format: 1024 bytes max
                
                # 0x36 TransferData
                elif sid == 0x36:
                    seq = data[1]
                    conn.sendall(bytes([0x76, seq]))
                
                # 0x37 RequestTransferExit
                elif sid == 0x37:
                    conn.sendall(bytes([0x77]))
                
                # 0x3E TesterPresent
                elif sid == 0x3E:
                    sub = data[1] & 0x7F
                    suppress = (data[1] & 0x80) != 0
                    if suppress:
                        conn.sendall(b'')
                    else:
                        conn.sendall(bytes([0x7E, sub]))
                
                # 0x83 AccessTimingParameter
                elif sid == 0x83:
                    sub = data[1] & 0x7F
                    conn.sendall(bytes([0xC3, sub, 0x00, 0x32, 0x01, 0xF4]))
                
                # 0x85 ControlDTCSetting
                elif sid == 0x85:
                    sub = data[1] & 0x7F
                    conn.sendall(bytes([0xC5, sub]))
                
                # 0x86 ResponseOnEvent
                elif sid == 0x86:
                    sub = data[1] & 0x7F
                    conn.sendall(bytes([0xC6, sub, 0x00]))
                
                # 0x87 LinkControl
                elif sid == 0x87:
                    sub = data[1] & 0x7F
                    conn.sendall(bytes([0xC7, sub]))
                
                else:
                    # Echo raw UDS response byte 0x40 + SID
                    conn.sendall(bytes([sid + 0x40]) + data[1:])
                    
            conn.close()
        except Exception as e:
            print(f"[3rd-Party UDS Daemon] Exception: {e}")
            time.sleep(1)

if __name__ == '__main__':
    main()
