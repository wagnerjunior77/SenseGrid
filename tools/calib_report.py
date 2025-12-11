#!/usr/bin/env python3
import argparse, json, sys, math
from pathlib import Path

# thresholds default
DEF_MAX_STD = 0.12  # snr jitter aceitavel
DEF_MIN_VALID = 0.8 # proporcao minima de amostras validas
DEF_MAX_RANGE_CM = 600

def load_lines(path):
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                pass

def is_calib_entry(obj):
    return isinstance(obj, dict) and obj.get("tag") == "calib"

def summarize(entries):
    rows = []
    for e in entries:
        meta = e.get("meta", {})
        metrics = e.get("metrics", {})
        sug = e.get("suggest", {})
        cur = e.get("current", {})
        row = {
            "ts": meta.get("ts_ms"),
            "state": meta.get("state"),
            "elapsed": metrics.get("elapsed_ms"),
            "valid_ratio": metrics.get("valid_ratio"),
            "snr_mean": metrics.get("snr_mean"),
            "snr_std": metrics.get("snr_std"),
            "dist_p95_cm": metrics.get("dist_p95_cm"),
            "range_cur": cur.get("max_range_cm"),
            "range_sug": sug.get("max_range_cm"),
            "hold_e_cur": cur.get("hold_empty_ms"),
            "hold_e_sug": sug.get("hold_empty_ms"),
        }
        rows.append(row)
    return rows

def analyze(rows, max_std, min_valid, max_range_cm):
    alerts = []
    for r in rows:
        if r["valid_ratio"] is not None and r["valid_ratio"] < min_valid:
            alerts.append(f"low valid_ratio ({r['valid_ratio']:.3f}) ts={r['ts']}")
        if r["snr_std"] is not None and r["snr_std"] > max_std:
            alerts.append(f"high snr_std ({r['snr_std']:.3f}) ts={r['ts']}")
        if r["dist_p95_cm"] is not None and r["dist_p95_cm"] > max_range_cm:
            alerts.append(f"dist_p95_cm above max_range ({r['dist_p95_cm']}) ts={r['ts']}")
    return alerts

def print_table(rows):
    if not rows:
        print("no calibration entries found")
        return
    header = ["ts_ms","state","valid_ratio","snr_mean","snr_std","dist_p95_cm","range_cur->sug","hold_empty_cur->sug"]
    print("\t".join(header))
    for r in rows:
        rc = r["range_cur"]; rs = r["range_sug"]
        hec = r["hold_e_cur"]; hes = r["hold_e_sug"]
        print(f"{r['ts']}\t{r['state']}\t{r['valid_ratio']:.3f}\t{r['snr_mean']:.3f}\t{r['snr_std']:.3f}\t{r['dist_p95_cm']}\t{rc}->{rs}\t{hec}->{hes}")

def main():
    ap = argparse.ArgumentParser(description="Resumo de logs de calibracao (.jsonl)")
    ap.add_argument("log", type=Path, help="arquivo .jsonl (ex.: logs/calib.jsonl)")
    ap.add_argument("--max-std", type=float, default=DEF_MAX_STD, help="limite de snr_std para alerta (default 0.12)")
    ap.add_argument("--min-valid", type=float, default=DEF_MIN_VALID, help="limite de valid_ratio para alerta (default 0.8)")
    ap.add_argument("--max-range-cm", type=float, default=DEF_MAX_RANGE_CM, help="limite de dist_p95_cm (default 600)")
    args = ap.parse_args()

    entries = [e for e in load_lines(args.log) if is_calib_entry(e)]
    rows = summarize(entries)
    alerts = analyze(rows, args.max_std, args.min_valid, args.max_range_cm)
    print_table(rows)
    if alerts:
        print("\nAlerts:")
        for a in alerts:
            print(f"- {a}")

if __name__ == "__main__":
    main()
