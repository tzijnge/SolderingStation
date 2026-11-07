"""Captures the station's telemetry for a few seconds and prints a summary.

Starts the "samples" stream of the RPC telemetry service over the USB serial
port, collects samples for --seconds, stops the stream and prints statistics
of the raw ADC value, the measured temperature and the duty, plus the faults
seen. For characterising the tip sensor input (issue #3) without reading
values off a plot. Read-only: it doesn't send any input to the station.

Ctrl+C ends the capture early and still prints the summary. With --csv every
sample (setpoint, measured, filtered, duty, adc_raw, fault) is written to the
file as it arrives, so a capture that is killed keeps what it has recorded.

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
    help="Also write every sample to this CSV file (all telemetry fields), to analyse the series. "
    "Written as the samples arrive, so it's complete up to the moment the capture stops, however it stops.",
)
def main(seconds: float, port: str | None, csv_path: Path | None) -> None:
    if port is None:
        raise click.UsageError("no --port given and no port in lrpcc.config.yaml")

    lrpc_def = load_lrpc_def(DEFINITION)

    raw: list[int] = []
    measured: list[float] = []
    duty: list[int] = []
    setpoint: list[int] = []
    faults: Counter[str] = Counter()
    received: list[float] = []

    csv_file = None
    if csv_path is not None:
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        csv_file = csv_path.open("w", encoding="utf-8")
        csv_file.write("t_s,setpoint,measured,filtered,duty,adc_raw,fault\n")
        csv_file.flush()

    try:
        with serial.Serial(port, timeout=2) as transport:
            client = LrpcClient(lrpc_def, transport)
            start = time.monotonic()
            try:
                for response in client.communicate_all("telemetry", "samples", start=True):
                    t = time.monotonic() - start
                    sample = response.payload
                    received.append(t)
                    raw.append(sample["adc_raw"])
                    measured.append(sample["measured"])
                    duty.append(sample["duty"])
                    setpoint.append(sample["setpoint"])
                    faults[str(sample["fault"])] += 1
                    if csv_file is not None:
                        # Flushed per sample (10 per second): a capture that is killed or crashes keeps
                        # everything up to that point.
                        csv_file.write(
                            f"{t:.3f},{sample['setpoint']},{sample['measured']:.2f},{sample['filtered']:.2f},"
                            f"{sample['duty']},{sample['adc_raw']},{sample['fault']}\n"
                        )
                        csv_file.flush()
                    if t >= seconds:
                        break
            except KeyboardInterrupt:
                # Ctrl+C ends the capture early; the summary of what was captured follows.
                click.echo("Interrupted, stopping the stream")
            finally:
                next(client.communicate_all("telemetry", "samples", start=False), None)
    finally:
        if csv_file is not None:
            csv_file.close()

    if not raw:
        raise click.ClickException("no samples received")

    click.echo(f"{len(raw)} samples in {received[-1]:.1f} s from {port}")
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
