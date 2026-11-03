"""Forwards the station's telemetry to Teleplot.

Starts the "samples" stream of the RPC telemetry service over the USB serial
port and sends each sample to Teleplot (https://teleplot.fr/) over UDP, as
`name:value` lines. Stops the stream on exit (Ctrl+C).

Usage, from the repo root (after `pip install -r tools/requirements.txt`):

    python tools/telemetry.py [--port COM5]

Without --port, the port is taken from lrpcc.config.yaml in the repo root
(see the README). The serial port can only be open in one program at a time,
so lrpcc can't be used while this runs.
"""

import argparse
import socket
from collections import deque
from pathlib import Path

import serial
import yaml
from lrpc.client import LrpcClient
from lrpc.utils import load_lrpc_def

REPO = Path(__file__).resolve().parent.parent
DEFINITION = REPO / "rpc" / "soldering_station.lrpc.yaml"
LRPCC_CONFIG = REPO / "lrpcc.config.yaml"

TELEPLOT = ("127.0.0.1", 47269)

# measuredAvg: 2 s moving average of the measured temperature (20 samples at
# 100 ms), which shows the trend under the sensor noise.
MOVING_AVG_SAMPLES = 20


def default_port() -> str | None:
    if not LRPCC_CONFIG.exists():
        return None
    config = yaml.safe_load(LRPCC_CONFIG.read_text(encoding="utf-8"))
    return config.get("transport_params", {}).get("port")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", default=default_port(), help="serial port of the station (default: from lrpcc.config.yaml)")
    args = parser.parse_args()
    if args.port is None:
        parser.error("no --port given and no port in lrpcc.config.yaml")

    lrpc_def = load_lrpc_def(DEFINITION)
    fault = lrpc_def.enum("Fault")

    # The timeout is how long a read may wait for the next sample (100 ms
    # apart); the client raises TimeoutError when it expires.
    with serial.Serial(args.port, timeout=2) as transport:
        client = LrpcClient(lrpc_def, transport)
        udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        measured_history: deque[float] = deque(maxlen=MOVING_AVG_SAMPLES)
        print(f"Forwarding telemetry from {args.port} to Teleplot at {TELEPLOT[0]}:{TELEPLOT[1]}, Ctrl+C to stop")
        try:
            for response in client.communicate_all("telemetry", "samples", start=True):
                sample = response.payload
                measured_history.append(sample["measured"])
                values = {
                    "setpoint": sample["setpoint"],
                    "measured": sample["measured"],
                    "measuredFiltered": sample["filtered"],
                    "measuredAvg": sum(measured_history) / len(measured_history),
                    "duty": sample["duty"],
                    "adcRaw": sample["adc_raw"],
                    "fault": fault.field_id(sample["fault"]),
                }
                # No ">" in front, unlike Teleplot over serial: over UDP, ">" marks a
                # log message, not a variable.
                udp.sendto("".join(f"{name}:{value}\n" for name, value in values.items()).encode(), TELEPLOT)
        except KeyboardInterrupt:
            pass
        finally:
            next(client.communicate_all("telemetry", "samples", start=False), None)
            print("Stopped")


if __name__ == "__main__":
    main()
