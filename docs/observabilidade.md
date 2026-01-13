# Observabilidade - esquema de telemetria (kpi, heatmap, log)

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
  "snr_p95": 0.80,
  "dist_p50_cm": 120,
  "dist_p95_cm": 280,
  "state_ratio": { "empty": 0.60, "presence": 0.30, "motion": 0.10 },
  "transition_count": 12,
  "stale_ratio": 0.02
}
```

Notas:
- `meas_valid` e `stale_ratio` ajudam a detectar perda de frames.
- percentis sao calculados sobre o periodo da janela.

## Heatmap (type = "heatmap")
Heatmap 1D por distancia (padrao para ME73 sem angulos confiaveis).

```json
{
  "type": "range_1d",
  "window_ms": 60000,
  "bin_cm": 50,
  "range_cm": 600,
  "metric": "count",
  "bins": [0, 2, 10, 4, 1, 0, 0, 0, 0, 0, 0, 0]
}
```

Campos:
- `bin_cm`: largura do bin em cm
- `range_cm`: range total coberto (bins * bin_cm)
- `metric`: `count` ou `snr_avg`
- `bins`: array com uma posicao por faixa de distancia

Extensao futura (sensores com angulo):
```json
{
  "type": "grid_2d",
  "grid_w": 3,
  "grid_h": 2,
  "bins": [0, 1, 4, 2, 0, 0]
}
```

## Endpoints e topicos (planejado)
HTTP:
- GET `/v1/kpi`
- GET `/v1/heatmap`
- GET `/v1/diagnostics/export` -> stream `.jsonl`

MQTT:
- `.../kpi` (type = "kpi")
- `.../heatmap` (type = "heatmap")

## Log estruturado (.jsonl)
Cada linha eh um envelope JSON. Tipos recomendados:
- `meas` (medida)
- `event` (mudanca de estado)
- `kpi`
- `heatmap`

Exemplo:
```json
{"v":1,"ts_ms":1000,"ts_iso":"1970-01-01T00:00:01.000Z","device_id":"sg-x","seq":1,"type":"kpi","payload":{"window_ms":60000,"meas_total":1200}}
```
