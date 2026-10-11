"""Lanza el núcleo C++ como subproceso y lee el JSON de salida."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional

ROOT = Path(__file__).resolve().parent.parent
BACKEND = ROOT / "backend"
SHARED = ROOT / "shared_data"
# Los escenarios de ejemplo viven FUERA del código, en ../documentacion/ejemplos (si existe).
EXAMPLES = ROOT.parent / "documentacion" / "ejemplos"
INPUT_PATH = SHARED / "input.json"
OUTPUT_PATH = SHARED / "output.json"


def simulator_binary() -> Path:
    exe = "sosim.exe" if sys.platform == "win32" else "sosim"
    candidates = [
        BACKEND / "build" / exe,
        BACKEND / "build" / "Release" / exe,
        BACKEND / "build" / "Debug" / exe,
        BACKEND / exe,
    ]
    for c in candidates:
        if c.exists():
            return c
    return candidates[0]


def ensure_built() -> Path:
    SHARED.mkdir(parents=True, exist_ok=True)
    binary = simulator_binary()
    if binary.exists():
        return binary
    build_dir = BACKEND / "build"
    build_dir.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["cmake", "-S", str(BACKEND), "-B", str(build_dir)], check=True)
        subprocess.run(["cmake", "--build", str(build_dir)], check=True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        subprocess.run(["make", "-C", str(BACKEND)], check=True)
    binary = simulator_binary()
    if not binary.exists():
        raise FileNotFoundError(
            "No se encontró el binario sosim. Compila con:\n"
            "  cmake -S backend -B backend/build && cmake --build backend/build"
        )
    return binary


def run_kernel(payload: Dict[str, Any], module: Optional[str] = None) -> Dict[str, Any]:
    binary = ensure_built()
    SHARED.mkdir(parents=True, exist_ok=True)
    INPUT_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    cmd = [str(binary), "-i", str(INPUT_PATH), "-o", str(OUTPUT_PATH)]
    if module:
        cmd += ["-m", module]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"El núcleo C++ falló (código {proc.returncode}):\n"
            f"{proc.stderr or proc.stdout}"
        )
    if not OUTPUT_PATH.exists():
        raise FileNotFoundError(f"No se generó {OUTPUT_PATH}")
    return json.loads(OUTPUT_PATH.read_text(encoding="utf-8"))


def default_scenario() -> Dict[str, Any]:
    demo = EXAMPLES / "demo_all.json"
    if demo.exists():
        return json.loads(demo.read_text(encoding="utf-8"))
    return {"module": "all"}  # sin ejemplos: el núcleo usa sus valores por defecto
