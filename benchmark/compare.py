#!/usr/bin/env python3
"""Run comparable PCO/pthread workloads, retaining parameters and raw results."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, help="path to pco_latency")
    parser.add_argument("--cpus", required=True, help="CPU list passed to taskset, e.g. 2 or 2-3")
    parser.add_argument("--output", required=True, help="new directory for experiment records")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--quantum-us", type=int, default=1000)
    parser.add_argument("--timeout", type=float, default=120,
                        help="maximum seconds per run; timeouts are recorded")
    parser.add_argument("extra", nargs=argparse.REMAINDER,
                        help="after --, additional options passed to pco_latency")
    args = parser.parse_args()
    if args.repeats < 1 or args.workers < 1 or args.quantum_us < 1 or args.timeout <= 0:
        parser.error("repeats, workers, quantum-us and timeout must be positive")
    extra = args.extra[1:] if args.extra[:1] == ["--"] else args.extra
    for token in extra:
        if token.split("=", 1)[0] in {"--mode", "--workers", "--quantum-us", "--max-klts", "--csv"}:
            parser.error(f"{token} is controlled by the comparison driver")
    binary = Path(args.binary).resolve()
    if not binary.is_file():
        parser.error(f"binary does not exist: {binary}")
    input_path = None
    if "--input" in extra:
        index = extra.index("--input")
        if index + 1 >= len(extra):
            parser.error("--input requires a path")
        input_path = Path(extra[index + 1]).resolve()
        if not input_path.is_file():
            parser.error(f"input file does not exist: {input_path}")
    out = Path(args.output)
    try:
        out.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        parser.error(f"output already exists: {out}")
    modes = [
        ("pco-coop", "pco-coop", None),
        ("pco-preempt-cap1", "pco-preempt", 1),
        ("pco-preempt-cap8", "pco-preempt", 8),
        ("pco-preempt-unbounded", "pco-preempt", 0),
        ("pthread-pool", "pthread-pool", None),
        ("pthread-task", "pthread-task", None),
    ]
    metadata = {
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "binary": str(binary), "binary_sha256": sha256_file(binary),
        "input_file": str(input_path) if input_path else None,
        "input_sha256": sha256_file(input_path) if input_path else None,
        "cpus": args.cpus, "workers": args.workers,
        "repeats": args.repeats, "quantum_us": args.quantum_us,
        "timeout_seconds": args.timeout, "extra_options": extra,
        "platform": platform.platform(), "kernel": platform.release(),
        "machine": platform.machine(),
        "note": "Generator shares the taskset CPU list. These are raw runs, not a formal performance conclusion.",
        "runs": [],
    }
    if hasattr(os, "sched_getaffinity"):
        metadata["launcher_allowed_cpus"] = sorted(os.sched_getaffinity(0))
    failures = 0
    for label, mode, cap in modes:
        for repeat in range(1, args.repeats + 1):
            stem = f"{label}-{repeat:03d}"
            csv = (out / (stem + ".csv")).resolve()
            command = ["taskset", "-c", args.cpus, str(binary), "--mode", mode,
                       "--workers", str(args.workers), "--long-us", "200000",
                       "--short-count", "100", "--interval-us", "1000",
                       "--quantum-us", str(args.quantum_us), "--csv", str(csv)]
            if cap is not None:
                command += ["--max-klts", str(cap)]
            command += extra
            print(stem, flush=True)
            run = {"name": stem, "argv": command}
            try:
                result = subprocess.run(command, capture_output=True, text=True,
                                        timeout=args.timeout, check=False)
                stdout, stderr = result.stdout, result.stderr
                run["returncode"] = result.returncode
                run["timeout"] = False
                if result.returncode:
                    failures += 1
            except subprocess.TimeoutExpired as exc:
                def decode(value):
                    return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")
                stdout, stderr = decode(exc.stdout), decode(exc.stderr)
                run["returncode"] = None
                run["timeout"] = True
                failures += 1
            (out / (stem + ".stdout.txt")).write_text(stdout, encoding="utf-8")
            (out / (stem + ".stderr.txt")).write_text(stderr, encoding="utf-8")
            run["csv_present"] = csv.is_file()
            metadata["runs"].append(run)
            (out / "metadata.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Wrote {len(metadata['runs'])} runs to {out}; failures or timeouts: {failures}.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
