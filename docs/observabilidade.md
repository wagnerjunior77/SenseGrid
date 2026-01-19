# Observabilidade - esquema de telemetria (kpi, log)

Objetivo: definir payloads e endpoints para diagnostico e observabilidade.
Todo texto novo em ASCII.

## Envelope padrao
Mesma estrutura do serializer:

```json
{"v":1,"ts_ms":47161,"ts_iso":"1970-01-01T00:00:47.161Z","device_id":"sg-90120803F784","seq":62,"type":"kpi","payload":{...}}
```

## KPI (type = "kpi")
Resumo de janela fixa (ex: 60s). Campos sugeridos:

```json
{
  "window_ms": 60000,
  "meas_total": 1200,
  "meas_valid": 1080,
  "snr_avg": 0.23,
  "latency_avg_ms": 52,
  "latency_max_ms": 140,
  "snr_p95": 0.80,
  "dist_p50_cm": 120,
  "dist_p95_cm": 280,
  "state_ratio": { "empty": 0.60, "presence": 0.30, "motion": 0.10 },
  "transition_count": 12,
  "fp_proxy_ratio": 0.03,
  "fn_proxy_ratio": 0.05,
  "stale_ratio": 0.02
}
```

Notas:
- `meas_valid` e `stale_ratio` ajudam a detectar perda de frames.
- percentis sao calculados sobre o periodo da janela.
- `fp/fn proxy` compara raw vs estado estabilizado; nao e ground truth.
- `latency_*` representa o delta entre frames (gap medio/max).

## Endpoints e topicos (planejado)
HTTP:
- GET `/v1/kpi`
- GET `/v1/diagnostics/kpi` (alias de /v1/kpi)
- GET `/v1/diagnostics/status` (alias de /v1/health)
- GET `/v1/diagnostics/export` -> stream `.jsonl`
- GET `/diagnostics` -> pagina HTML com cards de KPI

MQTT:
- `.../kpi` (type = "kpi")

## Log estruturado (.jsonl)
Cada linha eh um envelope JSON. Tipos recomendados:
- `meas` (medida)
- `event` (mudanca de estado)
- `kpi`

Exemplo:
```json
{"v":1,"ts_ms":1000,"ts_iso":"1970-01-01T00:00:01.000Z","device_id":"sg-x","seq":1,"type":"kpi","payload":{"window_ms":60000,"meas_total":1200}}
```
