"""PlatformIO pre-build script: generates the LotusRPC server code.

Runs before every build (`extra_scripts = pre:...` in platformio.ini):

1. Makes sure the LotusRPC version pinned in tools/requirements.txt is
   installed in a venv of its own (.pio/lrpc-venv), created from PlatformIO's
   Python. Not in PlatformIO's own environment: LotusRPC pins exact versions
   of packages PlatformIO uses too (e.g. click), which could break `pio`.
2. Runs `lrpcg cpp` on rpc/soldering_station.lrpc.yaml into
   $BUILD_DIR/lrpc - only when the definition, the pinned version or the
   output changed. Files whose content didn't change are left alone, so SCons
   doesn't recompile main.cpp for nothing.
3. Adds that directory to the include path.

Nothing generated is committed. A clean checkout builds without manual steps.
"""

import filecmp
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821 - provided by SCons

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
DEFINITION = PROJECT_DIR / "rpc" / "soldering_station.lrpc.yaml"
REQUIREMENTS = PROJECT_DIR / "tools" / "requirements.txt"
VENV = PROJECT_DIR / ".pio" / "lrpc-venv"
OUTPUT = Path(env.subst("$BUILD_DIR")) / "lrpc"  # noqa: F821
TOP_LEVEL_HEADER = "soldering_station.hpp"


def log(message: str) -> None:
    print(f"lrpc_generate: {message}")


def lotusrpc_requirement() -> str:
    """The lotusrpc line of tools/requirements.txt, e.g. 'lotusrpc[transport_serial]==1.0.0'."""
    for line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        requirement = line.split("#", 1)[0].strip()
        if requirement.lower().startswith("lotusrpc"):
            return requirement
    sys.exit(f"lrpc_generate: no lotusrpc requirement in {REQUIREMENTS}")


def venv_executable(name: str) -> Path:
    scripts = VENV / ("Scripts" if sys.platform == "win32" else "bin")
    return scripts / (f"{name}.exe" if sys.platform == "win32" else name)


def ensure_lotusrpc(requirement: str) -> None:
    python = venv_executable("python")
    if not python.exists():
        log(f"creating {VENV}")
        # Without pip: a venv made from PlatformIO's Python doesn't get a
        # working one. PlatformIO's own pip installs into it instead (--python).
        subprocess.check_call([sys.executable, "-m", "venv", "--without-pip", str(VENV)])

    # The requirement it was installed for, so a changed pin reinstalls.
    marker = VENV / "lotusrpc-requirement.txt"
    if marker.exists() and marker.read_text(encoding="utf-8") == requirement:
        return

    log(f"installing {requirement}")
    subprocess.check_call(
        [
            sys.executable,
            "-m",
            "pip",
            "--python",
            str(python),
            "install",
            "--quiet",
            "--disable-pip-version-check",
            requirement,
        ]
    )
    marker.write_text(requirement, encoding="utf-8")


def stamp(requirement: str) -> str:
    digest = hashlib.sha256(DEFINITION.read_bytes())
    digest.update(requirement.encode("utf-8"))
    return digest.hexdigest()


def sync(source: Path, destination: Path) -> None:
    """Make destination's files equal to source's, touching only those that differ."""
    destination.mkdir(parents=True, exist_ok=True)
    for path in source.rglob("*"):
        target = destination / path.relative_to(source)
        if path.is_dir():
            target.mkdir(exist_ok=True)
        elif not target.exists() or not filecmp.cmp(path, target, shallow=False):
            shutil.copy2(path, target)
    for path in sorted(destination.rglob("*"), reverse=True):
        if not (source / path.relative_to(destination)).exists() and path.name != ".stamp":
            if path.is_dir():
                path.rmdir()
            else:
                path.unlink()


def generate(requirement: str) -> None:
    stamp_file = OUTPUT / ".stamp"
    current = stamp(requirement)
    if (OUTPUT / TOP_LEVEL_HEADER).exists() and stamp_file.exists() and stamp_file.read_text() == current:
        return

    log(f"generating {DEFINITION.name} into {OUTPUT}")
    scratch = OUTPUT.with_name("lrpc.tmp")
    shutil.rmtree(scratch, ignore_errors=True)
    subprocess.check_call([str(venv_executable("lrpcg")), "cpp", "-d", str(DEFINITION), "-o", str(scratch)])
    sync(scratch, OUTPUT)
    shutil.rmtree(scratch)
    stamp_file.write_text(current)


requirement = lotusrpc_requirement()
ensure_lotusrpc(requirement)
generate(requirement)

# The generated code includes its external enums' headers relative to src/
# (e.g. "station/StationStateId.h").
env.Append(CPPPATH=[str(OUTPUT), str(PROJECT_DIR / "src")])  # noqa: F821
