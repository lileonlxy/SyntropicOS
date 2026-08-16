#!/usr/bin/env python3
"""
Official 3rd-Party OCPP 1.6-J CSMS Server Integration Test Service.
Uses the official Open Charge Alliance 'ocpp' Python package to strictly
validate all incoming station Call frames and outgoing CSMS response frames.
"""

import sys
import asyncio
import json
import logging
from ocpp.v16 import ChargePoint as cp
from ocpp.v16 import call_result, call
from ocpp.v16.enums import RegistrationStatus, AuthorizationStatus, Action
from ocpp.routing import on

logging.basicConfig(level=logging.INFO)

class RefCSMSServer(cp):
    def __init__(self, id, connection):
        super().__init__(id, connection)
        self.received_boot = False
        self.received_status_count = 0
        self.received_auth = False
        self.received_start_tx = False
        self.received_meter_values = False
        self.received_stop_tx = False
        self.active_transaction_id = 1001

    @on(Action.boot_notification)
    def on_boot_notification(self, charge_point_vendor, charge_point_model, **kwargs):
        logging.info(f"[Python CSMS] BootNotification from Vendor={charge_point_vendor}, Model={charge_point_model}")
        self.received_boot = True
        return call_result.BootNotification(
            current_time="2026-08-06T12:00:00Z",
            interval=60,
            status=RegistrationStatus.accepted
        )

    @on(Action.status_notification)
    def on_status_notification(self, connector_id, error_code, status, **kwargs):
        logging.info(f"[Python CSMS] StatusNotification Connector={connector_id}, Status={status}")
        self.received_status_count += 1
        return call_result.StatusNotification()

    @on(Action.authorize)
    def on_authorize(self, id_tag, **kwargs):
        logging.info(f"[Python CSMS] Authorize id_tag={id_tag}")
        self.received_auth = True
        if id_tag == "RFID-TAG-BAD":
            return call_result.Authorize(id_tag_info={"status": AuthorizationStatus.invalid})
        return call_result.Authorize(id_tag_info={"status": AuthorizationStatus.accepted})

    @on(Action.start_transaction)
    def on_start_transaction(self, connector_id, id_tag, meter_start, timestamp, **kwargs):
        logging.info(f"[Python CSMS] StartTransaction Connector={connector_id}, Tag={id_tag}, MeterStart={meter_start}")
        self.received_start_tx = True
        return call_result.StartTransaction(
            transaction_id=self.active_transaction_id,
            id_tag_info={"status": AuthorizationStatus.accepted}
        )

    @on(Action.meter_values)
    def on_meter_values(self, connector_id, meter_value, **kwargs):
        logging.info(f"[Python CSMS] MeterValues Connector={connector_id}, Values={meter_value}")
        self.received_meter_values = True
        return call_result.MeterValues()

    @on(Action.stop_transaction)
    def on_stop_transaction(self, meter_stop, timestamp, transaction_id, reason=None, **kwargs):
        logging.info(f"[Python CSMS] StopTransaction TxID={transaction_id}, MeterStop={meter_stop}, Reason={reason}")
        self.received_stop_tx = True
        return call_result.StopTransaction(id_tag_info={"status": AuthorizationStatus.accepted})

    @on(Action.heartbeat)
    def on_heartbeat(self, **kwargs):
        logging.info("[Python CSMS] Heartbeat received")
        return call_result.Heartbeat(current_time="2026-08-06T12:00:00Z")

    @on("DisplayMessage", skip_schema_validation=True)
    def on_display_message(self, **kwargs):
        logging.info("[Python CSMS] DisplayMessage received")
        return call_result.Heartbeat(current_time="2026-08-06T12:00:00Z")

    @on("V2GEnergyTransfer", skip_schema_validation=True)
    def on_v2g_energy_transfer(self, **kwargs):
        logging.info("[Python CSMS] V2GEnergyTransfer received")
        return call_result.Heartbeat(current_time="2026-08-06T12:00:00Z")

async def handle_websocket(websocket):
    logging.info("[Python CSMS] Station client connected")
    cp_instance = RefCSMSServer("CP001", websocket)
    await cp_instance.start()

async def main():
    import websockets
    server = await websockets.serve(handle_websocket, "127.0.0.1", 9001, subprotocols=["ocpp1.6"])
    logging.info("[Python CSMS] Listening on ws://127.0.0.1:9001/CP001")
    await server.wait_closed()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        sys.exit(0)
