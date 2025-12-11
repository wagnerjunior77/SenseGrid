#!/usr/bin/env python3
"""
tools/metrics.py

Resumo rápido de um log .jsonl gerado pelo SenseGrid (stream do radar).
Extrai o estado (stable/state/status), calcula duração por estado,
conta transições e flickers (mudanças rápidas).

Uso:
  python tools/metrics.py --file logs/session.jsonl [--flicker-ms 500]
"""
import argparse
import json
import sys
from collections import defaultdict


STATE_STR_TO_INT = {
    "none": 0,
    "empty": 0,
    "exist": 1,
    "presence": 1,
    "present": 1,
    "move": 2,
    "motion": 2,
}


def normalize_state(entry):
    """Tenta extrair o estado da linha."""
    for key in ("stable", "state", "status"):
        if key in entry:
            raw = entry[key]
            if isinstance(raw, (int, float)):
                try:
                    return int(raw)
                except Exception:
                    return None
            if isinstance(raw, str):
                raw_norm = raw.strip().lower()
                if raw_norm in STATE_STR_TO_INT:
                    return STATE_STR_TO_INT[raw_norm]
                # tenta parse numérico em string
                try:
                    return int(float(raw_norm))
                except Exception:
                    return None
    return None


def extract_ts_ms(entry):
    """Prefere ts_ms; fallback para 'ts' ou 'timestamp_ms'."""
    for key in ("ts_ms", "timestamp_ms", "ts"):
        if key in entry:
            try:
                return int(entry[key])
            except Exception:
                try:
                    return int(float(entry[key]))
                except Exception:
                    pass
    return None


def format_ms(ms):
    s = ms / 1000.0
    return f"{ms} ms ({s:.2f} s)"


def main():
    ap = argparse.ArgumentParser(description="Resumo de estados de um log JSONL do SenseGrid.")
    ap.add_argument("--file", required=True, help="logs/session.jsonl")
    ap.add_argument("--flicker-ms", type=int, default=500, help="Janela p/ contar flicker (mudança e volta).")
    args = ap.parse_args()

    total = 0
    transitions = 0
    flicker = 0
    durations = defaultdict(int)  # state -> ms

    prev_state = None
    prev_ts = None
    last_change_ts = None

    try:
        with open(args.file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                except Exception:
                    continue
                ts = extract_ts_ms(obj)
                state = normalize_state(obj)
                if ts is None or state is None:
                    continue
                total += 1

                if prev_state is not None and prev_ts is not None:
                    dt = ts - prev_ts
                    if dt > 0:
                        durations[prev_state] += dt
                if prev_state is not None and state != prev_state:
                    transitions += 1
                    if last_change_ts is not None and (ts - last_change_ts) <= args.flicker_ms:
                        flicker += 1
                    last_change_ts = ts

                prev_state = state
                prev_ts = ts
    except FileNotFoundError:
        print(f"[ERR] arquivo não encontrado: {args.file}", file=sys.stderr)
        sys.exit(1)

    print(f"[metrics] arquivo: {args.file}")
    print(f"total_amostras: {total}")
    print(f"transicoes: {transitions}")
    print(f"flicker(<= {args.flicker_ms} ms): {flicker}")
    print("duracao_por_estado:")
    for st in (0, 1, 2):
        ms = durations.get(st, 0)
        print(f"  state={st}: {format_ms(ms)}")
    outros = {k: v for k, v in durations.items() if k not in (0, 1, 2)}
    if outros:
        for k, v in outros.items():
            print(f"  state={k}: {format_ms(v)}")


if __name__ == "__main__":
    main()
