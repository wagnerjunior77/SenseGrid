# Pipeline v1 (SenseGrid) — A3

> Detecção empty/presence/motion com gate de distância/SNR, holds e baseline EMA.

## State machine (hold/histerese)

```
               (hold_motion)
     +-----------------------------+
     |                             |
     v                             |
  MOTION  --(hold_empty)-->  EMPTY |
     ^           ^                 |
     |           |                 |
     +----(hold_exist)---- PRESENCE+
```

- **Gates antes da máquina**: distância `<= max_range_cm`, `snr >= snr_min`. Fora disso, estado é forçado a `EMPTY`.
- **Escalonamento MOTION**: `status=move` do radar **e** `snr >= snr_move` **e** `|speed| >= speed_thr_cms`. Caso contrário, cai para PRESENCE/EMPTY.
- **Baseline**: EMA `b = (1-k_ema)*b + k_ema*snr`. PRESENCE só entra se `snr >= baseline + delta_exist`.
- **Holds**: trocas respeitam `hold_empty_ms`, `hold_exist_ms`, `hold_motion_ms`.

## Fórmulas usadas

- **median3(x[n-2], x[n-1], x[n])**: filtra ruído impulsivo.
- **IIR 1ª ordem**: `y[n] = a*y[n-1] + (1-a)*x[n]` (usado em SNR/vel).
- **EMA baseline**: `b = (1-k_ema)*b + k_ema*x` (x = SNR atual).

## Parâmetros default (sg_pipe_init)

| Parâmetro          | Default | CLI                   | Efeito                               |
|--------------------|---------|-----------------------|--------------------------------------|
| max_range_cm       | 200     | `range 2|4|6` / `pipe set dist_max <cm>` | Gate de distância                     |
| hold_empty_ms      | 600     | `pipe set hold empty <ms>`    | Tempo para consolidar EMPTY          |
| hold_exist_ms      | 250     | `pipe set hold presence <ms>` | Tempo para consolidar PRESENCE       |
| hold_motion_ms     | 150     | `pipe set hold motion <ms>`   | Tempo para consolidar MOTION         |
| snr_on_exist       | 0.10    | —                     | Limiar de entrada em PRESENCE        |
| snr_off_exist      | 0.05    | —                     | Limiar de saída de PRESENCE          |
| snr_min            | 0.05    | `pipe set snr_min <0..1>`     | Gate duro                             |
| snr_move           | 0.15    | `pipe set snr_move <0..1>`    | SNR mínimo para MOTION               |
| delta_exist        | 0.05    | `pipe set delta_exist <0..1>` | Offset acima do baseline para PRESENCE |
| speed_thr_cms      | 5       | `pipe set speed_thr <cm/s>`   | |speed| mínimo para MOTION           |
| k_ema              | 0.02    | `pipe set k_ema <0.001..0.2>` | Ganho do baseline EMA                |

## CLI — exemplos rápidos

```
range 4               # preset 4 m (hardware+pipeline)
pipe show             # dump JSON dos params (NVS)
pipe set dist_max 200 # força 2 m
pipe set snr_min 0.08
pipe set speed_thr 8
pipe set hold presence 400
pipe off              # bypass pipeline (não recomendado para produção)
stream on             # liga streaming JSON
json on               # garante JSON por linha
```

## Amostra real (session.jsonl)

Excerto com estado estável (≈54 s) da sessão gravada em `logs/session.jsonl`:

```json
{"ts_ms":57609,"status":"exist","dist_m":0.87,"speed_mps":0.0,"snr":0.412,"distance_cm":87,"speed_cms":0,"signal":844,"az_deg":0,"el_deg":0,"state":1,"stable":1,"stable_ms":54494,"in_range":true}
```

> Nota: ainda falta uma sessão com `stable` ≥ 10 min para compor o exemplo definitivo; ao gravar, substituir o trecho acima por um bloco dessa sessão longa.

## Playbook de tuning

- **Falsos positivos longe**: reduza `dist_max` (range 2 m) e suba `snr_min`/`delta_exist`.
- **Perde presença sutil**: baixe `snr_min` (ex.: 0.03) e `delta_exist` (ex.: 0.03); aumente `hold_exist_ms` (ex.: 400 ms).
- **MOTION demais**: suba `snr_move` (ex.: 0.25) e `speed_thr_cms` (ex.: 10); aumente `hold_motion_ms`.
- **Troca rápida empty/presence**: aumente `hold_empty_ms` (ex.: 800–1200 ms) e `snr_off_exist` (ex.: 0.07).
- **Lento para detectar**: reduza holds (exist/motion) e aumente `k_ema` (acelera baseline).

## Checklist de uso

1) Escolha range: `range 2|4|6`.  
2) Ajuste SNR/holds conforme ambiente (ver playbook).  
3) Habilite streaming: `stream on`, `json on`.  
4) Valide com logger: `python tools/serial_logger.py --port COM5 --baud 115200 --out logs/session.jsonl`.  
5) Rode métricas: `python tools/metrics.py --file logs/session.jsonl`.  
6) Salve os ajustes relevantes no repositório (NVS já persiste no dispositivo).
