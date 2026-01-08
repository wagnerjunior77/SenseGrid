# Radar protocol (ME73MS01 family)

> Objective: document frame layout, decoded fields, and checksum rules.

---

## Frame layout (downlink to ESP)

```
Header   LEN (BE)   FUNC  CMD1  CMD2   DATA...   SUM
0x55 0xA5  0x00 0xNN   [1]   [1]   [1]   [N-4]     [1]
```

- Header: constant `0x55 0xA5`
- LEN: size in big-endian of the block `FUNC..SUM`
- FUNC, CMD1, CMD2: report or command identifiers
- DATA: report payload
- SUM: 1-byte checksum (mod 256)

### Checksum rules

- legacy: `FUNC + CMD1 + CMD2 + all DATA bytes`
- full: sum of `header + len + payload` (except `SUM`)
- firmware accepts both; invalid SUM is discarded

---

## Example (hex) and parsing

```
55 A5 00 0E  03 81 00 00  01 00 91 00  00 00 00 01  A5  C4
```

Mapping used in firmware for `FUNC=0x03`, `CMD1=0x81` (presence report):
- `target_id`   <- `DATA[0]`
- `status`      <- `DATA[1]` (`0=none`, `1=move`, `2=exist`)
- `distance_cm` <- `DATA[2..3]` (BE)
- `speed_cms`   <- `DATA[4..5]` (BE, int16)
- `az_deg`      <- `DATA[6]` (int8)
- `el_deg`      <- `DATA[7]` (int8)
- `signal`      <- `DATA[8..9]` (BE)

Notes:
- layout can vary by firmware revision
- parser ignores frames with invalid LEN or checksum

---

## Config commands used on boot

1. VO hold = 3000 ms
2. Presence max distance (ex: 200 cm)
3. Save all parameters

> Exact bytes can vary by module revision.
> Prefer `range <cm>` in the CLI and let the firmware build the frame.

---

## Status mapping

- `0 = none` -> empty
- `1 = move` -> motion detected
- `2 = exist` -> static presence

---

## Range notes

- Presence max is applied inside the module (ex: 2 m).
- Larger values are possible, but the default project setup uses 2 m
  to reduce interference and false positives in small rooms.
