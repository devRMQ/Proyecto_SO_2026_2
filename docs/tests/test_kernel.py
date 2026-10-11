#!/usr/bin/env python3
"""Pruebas de caja negra contra el binario C++."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

# Estructura: SO-Sim/so-simulator (código) y SO-Sim/documentacion/pruebas (este archivo)
CODE = Path(__file__).resolve().parent.parent.parent / "so-simulator"
BIN = CODE / "backend" / "build" / ("sosim.exe" if os.name == "nt" else "sosim")


def run(payload):
    with tempfile.TemporaryDirectory() as td:
        inp = Path(td) / "in.json"
        outp = Path(td) / "out.json"
        inp.write_text(json.dumps(payload), encoding="utf-8")
        r = subprocess.run([str(BIN), "-i", str(inp), "-o", str(outp)], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        return json.loads(outp.read_text(encoding="utf-8"))


def test_fcfs():
    data = run({
        "module": "schedule",
        "schedule": {
            "algorithm": "FCFS",
            "compare": False,
            "processes": [
                {"pid": 1, "name": "P1", "arrival": 0, "burst": 24, "priority": 1},
                {"pid": 2, "name": "P2", "arrival": 0, "burst": 3, "priority": 1},
                {"pid": 3, "name": "P3", "arrival": 0, "burst": 3, "priority": 1},
            ],
        },
    })["schedule"]
    by_pid = {p["pid"]: p for p in data["processes"]}
    assert by_pid[1]["waiting"] == 0
    assert by_pid[2]["waiting"] == 24
    assert by_pid[3]["waiting"] == 27
    assert abs(data["avg_waiting"] - 17.0) < 1e-6


def test_sjf():
    data = run({
        "module": "schedule",
        "schedule": {
            "algorithm": "SJF",
            "compare": False,
            "processes": [
                {"pid": 1, "name": "P1", "arrival": 0, "burst": 24, "priority": 1},
                {"pid": 2, "name": "P2", "arrival": 0, "burst": 3, "priority": 1},
                {"pid": 3, "name": "P3", "arrival": 0, "burst": 3, "priority": 1},
            ],
        },
    })["schedule"]
    by_pid = {p["pid"]: p for p in data["processes"]}
    assert by_pid[2]["waiting"] == 0
    assert by_pid[3]["waiting"] == 3
    assert by_pid[1]["waiting"] == 6
    assert abs(data["avg_waiting"] - 3.0) < 1e-6


def test_fifo_pages():
    refs = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1]
    data = run({
        "module": "memory",
        "memory": {
            "contiguous": {"total_memory": 32, "strategy": "FIRST_FIT", "requests": []},
            "paging": {"frames": 3, "compare": True, "references": refs},
        },
    })["memory"]["paging"]["results"]
    by_algo = {r["algorithm"]: r["page_faults"] for r in data}
    assert by_algo["FIFO"] == 15
    assert by_algo["LRU"] == 12
    assert by_algo["OPTIMAL"] == 9


def test_sync_invariants():
    data = run({
        "module": "sync",
        "sync": {
            "producers": 2, "consumers": 2, "buffer_size": 3,
            "items_per_producer": 4, "items_per_consumer": 4, "realtime": False,
        },
    })["sync"]
    inv = data["invariants"]
    assert inv["no_overflow"] is True
    assert inv["no_underflow"] is True
    assert inv["no_deadlock"] is True


# ------------------------------------------------------------------ SO integrado ("os")
REFS = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1]


def run_os(os_cfg):
    return run({"module": "os", "os": os_cfg})["os"]


def test_os_fcfs_contiguous():
    d = run_os({
        "scheduler": {"algorithm": "FCFS"},
        "memory": {"mode": "contiguous", "total_memory": 100},
        "processes": [
            {"pid": 1, "name": "P1", "burst": 24, "mem_size": 10},
            {"pid": 2, "name": "P2", "burst": 3, "mem_size": 10},
            {"pid": 3, "name": "P3", "burst": 3, "mem_size": 10},
        ],
    })
    assert abs(d["metrics"]["avg_waiting"] - 17.0) < 1e-6
    assert d["deadlock"] is False


def test_os_page_faults_classic():
    got = {}
    for algo in ("FIFO", "LRU", "OPTIMAL"):
        d = run_os({
            "scheduler": {"algorithm": "FCFS"},
            "memory": {"mode": "paged", "frames": 3, "replacement": algo, "page_fault_penalty": 1},
            "processes": [{"pid": 1, "name": "P", "burst": 20, "pages": 8, "refs": REFS}],
        })
        got[algo] = d["memory"]["page_faults"]
    assert got == {"FIFO": 15, "LRU": 12, "OPTIMAL": 9}, got


def test_os_page_fault_blocks_process():
    d = run_os({
        "scheduler": {"algorithm": "FCFS"},
        "memory": {"mode": "paged", "frames": 2, "replacement": "FIFO", "page_fault_penalty": 4},
        "processes": [{"pid": 1, "name": "P", "burst": 3, "pages": 3, "refs": [0, 1, 2]}],
    })
    p = d["processes"][0]
    assert p["page_faults"] == 3
    assert p["blocked_pf"] == 3 * 3  # cada fallo bloquea (penalización - 1) ticks tras el trap
    assert p["turnaround"] == 3 + 3 * 4  # 3 ticks de CPU + 4 por fallo


def test_os_memory_wait_and_fragmentation():
    d = run_os({
        "scheduler": {"algorithm": "FCFS"},
        "memory": {"mode": "contiguous", "total_memory": 20, "strategy": "FIRST_FIT"},
        "processes": [
            {"pid": 1, "name": "A", "arrival": 0, "burst": 4, "mem_size": 12},
            {"pid": 2, "name": "B", "arrival": 1, "burst": 2, "mem_size": 12},
            {"pid": 3, "name": "C", "arrival": 0, "burst": 1, "mem_size": 50},
        ],
    })
    by = {p["pid"]: p for p in d["processes"]}
    assert by[3]["state"] == "REJECTED"
    assert by[2]["mem_wait"] > 0 and by[2]["state"] == "TERMINATED"


def test_os_sync_producer_consumer():
    d = run_os({
        "scheduler": {"algorithm": "RR", "quantum": 2},
        "memory": {"mode": "paged", "frames": 8, "replacement": "LRU", "page_fault_penalty": 1},
        "sync": {"buffer_size": 2},
        "processes": [
            {"pid": 1, "name": "Prod", "burst": 8, "role": "producer", "pages": 2},
            {"pid": 2, "name": "Cons", "burst": 8, "role": "consumer", "pages": 2},
        ],
    })
    s = d["sync"]
    assert s["produced"] == 8 and s["consumed"] == 8
    assert s["overflow"] == 0 and s["underflow"] == 0 and s["max_occupancy"] <= 2
    assert s["invariants_ok"] is True and d["deadlock"] is False
    for f in d["frames"]:
        assert 0 <= f["sync"]["buffer"] <= 2


def test_os_deadlock_detected():
    d = run_os({
        "scheduler": {"algorithm": "FCFS"},
        "memory": {"mode": "contiguous", "total_memory": 64},
        "sync": {"buffer_size": 2},
        "processes": [{"pid": 1, "name": "Cons", "burst": 3, "role": "consumer", "mem_size": 8}],
    })
    assert d["deadlock"] is True


def test_os_frames_never_exceed_capacity():
    d = run({"module": "os", "os": {
        "scheduler": {"algorithm": "RR", "quantum": 3},
        "memory": {"mode": "paged", "frames": 4, "replacement": "FIFO", "page_fault_penalty": 2},
        "processes": [{"pid": i, "name": f"P{i}", "arrival": i, "burst": 9, "pages": 6} for i in range(1, 5)],
    }})["os"]
    for f in d["frames"]:
        assert len(f["memory"]["frames"]) == 4
        used = [(m["pid"], m["page"]) for m in f["memory"]["frames"] if m["pid"] != -1]
        assert len(used) == len(set(used))  # ninguna página duplicada en RAM


def test_os_compare_present():
    d = run_os({
        "compare": True,
        "memory": {"mode": "paged", "frames": 3, "page_fault_penalty": 1},
        "processes": [{"pid": 1, "name": "P", "burst": 20, "pages": 8, "refs": REFS}],
    })
    assert [c["algorithm"] for c in d["comparison"]["schedulers"]] == ["FCFS", "SJF", "RR", "PRIORITY"]
    assert {c["algorithm"]: c["page_faults"] for c in d["comparison"]["replacement"]} == {
        "FIFO": 15, "LRU": 12, "OPTIMAL": 9}


def test_os_thrashing_still_terminates():
    """Más procesos que marcos: debe terminar (una página recién cargada no se expulsa antes de usarse)."""
    procs = [{"pid": i, "name": f"P{i}", "burst": 6, "pages": 3} for i in range(1, 8)]
    for frames in (1, 2, 3, 6):
        for rep in ("FIFO", "LRU", "OPTIMAL"):
            d = run_os({
                "scheduler": {"algorithm": "RR", "quantum": 2},
                "memory": {"mode": "paged", "frames": frames, "replacement": rep, "page_fault_penalty": 3},
                "processes": procs,
            })
            assert d["timeout"] is False and d["metrics"]["completed"] == 7, (frames, rep)


def main():
    if not BIN.exists():
        print("Compila primero el núcleo.", file=sys.stderr)
        return 1
    test_fcfs()
    test_sjf()
    test_fifo_pages()
    test_sync_invariants()
    for name, fn in sorted(globals().items()):
        if name.startswith("test_os_") and callable(fn):
            fn()
    print("OK: laboratorios 1-3 + SO integrado (planificación, memoria, sincronización)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
