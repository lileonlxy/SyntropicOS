# STM32 UDS Application DTC Manager Demo

Demonstrates how to build a complete, zero-heap **Application-Layer Diagnostic Trouble Code (DTC) Manager** on top of the SyntropicOS UDS diagnostic stack (`syn_uds` / `syn_isotp`).

## Key Features

- **ISO 14229-1 Status Byte Engine**: Manages `testFailed`, `pendingDTC`, `confirmedDTC`, and self-healing transitions via non-blocking protothread tasks (`SYN_PT`).
- **Freeze-Frame / Snapshot Capture**: Captures system voltage, uptime, and diagnostic parameters upon fault occurrence.
- **UDS $19 ReadDTCInformation Sub-function Handlers**:
  - `0x01` `reportNumberOfDTCByStatusMask`
  - `0x02` `reportDTCByStatusMask`
  - `0x04` `reportDTCSnapshotIdentification`
  - `0x06` `reportDTCSnapshotRecordByDTCNumber`
- **UDS $14 ClearDiagnosticInformation Handler**: Clears active DTC records and resets snapshot memory.
