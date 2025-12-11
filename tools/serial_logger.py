#!/usr/bin/env python3
import argparse, sys, json, time
from datetime import datetime
import serial

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="COM5 / /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out",  required=True, help="logs/session.jsonl")
    ap.add_argument("--strip", action="store_true", help="remove prints não-JSON")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=1)
    print(f"[logger] {args.port} @ {args.baud} → {args.out}", file=sys.stderr)

    with open(args.out, "a", encoding="utf-8") as f:
        while True:
            line = ser.readline().decode(errors="replace").strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except Exception:
                if args.strip:
                    continue
                else:
                    obj = {"raw": line}
            obj["_host_ts"] = int(time.time() * 1000)
            obj["_host_iso"] = datetime.utcnow().isoformat() + "Z"
            f.write(json.dumps(obj, ensure_ascii=False) + "\n")
            f.flush()

if __name__ == "__main__":
    main()
