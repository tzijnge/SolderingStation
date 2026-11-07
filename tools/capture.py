"""Captures the station's telemetry for a few seconds and prints a summary.

Starts the "samples" stream of the RPC telemetry service over the USB serial
port, collects samples for --seconds, stops the stream and prints statistics
of the raw ADC value, the measured temperature and the duty, plus the faults
seen. For characterising the tip sensor input (issue #3) without reading
values off a plot. Read-only: it doesn't send any input to the station.

Usage, from the repo root (after `pip install -r tools/requirements.txt`):

    python tools/capture.py [--seconds 10] [--port COM5] [--csv samples.csv]

Without --port, the port is taken from lrpcc.config.yaml in the repo root
(see the README). The serial port can only be open in one program at a time,
so this can't run while tools/telemetry.py or lrpcc holds it.
"""

import statistics
import time
from collections import Counter
from pathlib import Path

import click
import serial
import yaml
from lrpc.client import LrpcClient
from lrpc.utils import load_lrpc_def

REPO = Path(__file__).resolve().parent.parent
DEFINITION = REPO / "rpc" / "soldering_station.lrpc.yaml"
LRPCC_CONFIG = REPO / "lrpcc.config.yaml"

# How many of the lowest distinct raw values to list.
LOW_VALUES_SHOWN = 8


def default_port() -> str | None:
    if not LRPCC_CONFIG.exists():
        return None
    config = yaml.safe_load(LRPCC_CONFIG.read_text(encoding="utf-8"))
    return config.get("transport_params", {}).get("port")


def describe(name: str, values: list[float], digits: int = 1) -> str:
    return (
        f"{name:<10} min {min(values):.{digits}f}  max {max(values):.{digits}f}  "
        f"mean {statistics.fmean(values):.{digits}f}  stdev {statistics.pstdev(values):.{digits}f}"
    )


@click.command(help=__doc__)
@click.option("--seconds", default=10.0, show_default=True, help="How long to capture.")
@click.option("--port", default=default_port, help="Serial port of the station.  [default: from lrpcc.config.yaml]")
@click.option(
    "--csv",
    "csv_path",
    type=click.Path(dir_okay=False, path_type=Path),
    help="Also write every sample to this CSV file (time received, raw, measured, duty), to analyse the series.",
)
def main(seconds: float, port: str | None, csv_path: Path | None) -> None:
    if port is None:
        raise click.UsageError("no --port given and no port in lrpcc.config.yaml")

    lrpc_def = load_lrpc_def(DEFINITION)
    fault = lrpc_def.enum("Fault")

    raw: list[int] = []
    measured: list[float] = []
    duty: list[int] = []
    setpoint: list[int] = []
    faults: Counter[str] = Counter()
    received: list[float] = []

    with serial.Serial(port, timeout=2) as transport:
        client = LrpcClient(lrpc_def, transport)
        start = time.monotonic()
        deadline = start + seconds
        try:
            for response in client.communicate_all("telemetry", "samples", start=True):
                received.append(time.monotonic() - start)
                sample = response.payload
                raw.append(sample["adc_raw"])
                measured.append(sample["measured"])
                duty.append(sample["duty"])
                setpoint.append(sample["setpoint"])
                faults[str(sample["fault"])] += 1
                if time.monotonic() >= deadline:
                    break
        finally:
            next(client.communicate_all("telemetry", "samples", start=False), None)

    if not raw:
        raise click.ClickException("no samples received")

    if csv_path is not None:
        lines = ["t_s,adc_raw,measured,duty"]
        lines += [f"{t:.3f},{r},{m:.2f},{d}" for t, r, m, d in zip(received, raw, measured, duty)]
        csv_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    click.echo(f"{len(raw)} samples in {seconds:g} s from {port}")
    click.echo(describe("adc_raw", raw, 1))
    click.echo(describe("measured", measured, 1))
    click.echo(describe("duty", duty, 0))
    click.echo(f"setpoint   {sorted(set(setpoint))} (0 = not On)")
    click.echo(f"faults     {dict(faults)}")
    counts = Counter(raw)
    lowest = sorted(counts)[:LOW_VALUES_SHOWN]
    click.echo("lowest raw " + "  ".join(f"{value}x{counts[value]}" for value in lowest))
    click.echo(f"highest raw {max(raw)}")


if __name__ == "__main__":
    main()
