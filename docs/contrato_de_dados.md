# Contrato de Payload JSON

**Projeto:** SenseGrid — Contrato de Dados  
**Versão:** v0.1 · **Compatibilidade:** alterações futuras devem ser **aditivas** (não quebrar campos existentes).

---

## 1. Envelope (comum a todas as mensagens)

```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:03:12Z",
  "device_id": "radar-01",
  "seq": 201,
  "type": "meas|event|status|cap|ack|err|cmd",
  "payload": {}
}
```

- **v:** versão do contrato  
- **ts:** timestamp ISO-8601 UTC  
- **device_id:** identificador do dispositivo  
- **seq:** sequência monotônica (uint32)  
- **type:** tipo da mensagem  
- **payload:** corpo específico do tipo

---

## 2. MQTT & HTTP

**MQTT (tópicos):**

- OUT:
  - `devices/{device_id}/meas`
  - `devices/{device_id}/events`
  - `devices/{device_id}/status`
  - `devices/{device_id}/cap`
  - `devices/{device_id}/ack`
  - `devices/{device_id}/err`
- IN:
  - `devices/{device_id}/cmd`

**HTTP (opcional, mesmo payload):**

- `GET /v1/occupancy` → estado agregado  
- `GET /v1/tracks` → tracks ativos  
- `POST /v1/cmd` → envia comando (payload idêntico ao do MQTT `cmd`)  
- `GET /v1/health` → status básico

---

## 3. Tipos de mensagem & exemplos

### 3.1 Medições (`type="meas"`)

Telemetria periódica (radar + ambientais).  
**Frequência:** 5–20 Hz (ajustável) ou agregada por janela.

```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:03:12Z",
  "device_id": "radar-01",
  "seq": 210,
  "type": "meas",
  "payload": {
    "measures": [
      { "sensor": "radar", "qty": "distance", "value": 1.12, "unit": "m" },
      { "sensor": "radar", "qty": "speed", "value": 0.03, "unit": "m/s" },
      { "sensor": "radar", "qty": "signal", "value": 385, "unit": "au" },
      { "sensor": "env", "qty": "temperature", "value": 27.1, "unit": "degC" },
      { "sensor": "env", "qty": "humidity", "value": 52.0, "unit": "%" },
      { "sensor": "env", "qty": "lux", "value": 120, "unit": "lx" }
    ],
    "status": 2,                  // 0=empty, 1=move, 2=still
    "dircos_deg": -3,
    "pitch_deg": 9,
    "target_id": 0                // ID técnico do alvo principal (se houver)
  }
}
```

### 3.2 Eventos (`type="event"`)

**Contagem mudou**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:03:13Z",
  "device_id": "radar-01",
  "seq": 211,
  "type": "event",
  "payload": { "class": "count.changed", "count": 5, "confidence": 0.93 }
}
```

**Início de track**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:03:11Z",
  "device_id": "radar-01",
  "seq": 212,
  "type": "event",
  "payload": { "class": "track.started", "target_id": "12", "zone": "centro", "method": "gate_cross" }
}
```

**Fim de track**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T15:17:42Z",
  "device_id": "radar-01",
  "seq": 278,
  "type": "event",
  "payload": { "class": "track.ended", "target_id": "12", "reason": "gate_out" }
}
```

**Atividade por zona**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:05:00Z",
  "device_id": "radar-01",
  "seq": 230,
  "type": "event",
  "payload": { "class": "zone.activity", "zones": { "A": 0.2, "B": 0.8, "C": 0.1 } }
}
```

**KPI periódica**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:05:05Z",
  "device_id": "radar-01",
  "seq": 235,
  "type": "event",
  "payload": { "class": "kpi.tick", "latency_ms": 180, "fn_rate": 0.03, "fp_rate": 0.02, "mae": 0.2 }
}
```

### 3.3 Status (`type="status"`)

```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:10:00Z",
  "device_id": "radar-01",
  "seq": 300,
  "type": "status",
  "payload": { "fw": "1.0.0", "uptime_s": 123456, "rssi_dbm": -55, "use_case": "sala_reuniao" }
}
```

### 3.4 Capacidades (`type="cap"`)

Inclui **schema** de configuração (para UI e validação).

```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:00:00Z",
  "device_id": "radar-01",
  "seq": 1,
  "type": "cap",
  "payload": {
    "sensors": [
      { "name": "radar", "measures": ["distance:m", "speed:m/s", "signal:au"], "events": ["presence", "count", "track"] },
      { "name": "env", "measures": ["temperature:degC", "humidity:%", "lux:lx"] }
    ],
    "config_schema": {
      "presence.max_distance_m":   { "type": "number",  "min": 0.5,  "max": 6,   "default": 3.5 },
      "motion.max_distance_m":     { "type": "number",  "min": 0.5,  "max": 6,   "default": 5.0 },
      "hysteresis_frames_enter":   { "type": "integer", "min": 1,    "max": 12,  "default": 5 },
      "hysteresis_frames_exit":    { "type": "integer", "min": 1,    "max": 20,  "default": 12 },
      "still_frames":              { "type": "integer", "min": 10,   "max": 200, "default": 80 },
      "vel_threshold_mps":         { "type": "number",  "min": 0.05, "max": 0.5, "default": 0.15 },
      "count.gate_position_m":     { "type": "number",  "min": 0.1,  "max": 1.5, "default": 0.5 },
      "count.gate_width_m":        { "type": "number",  "min": 0.5,  "max": 2.0, "default": 0.9 },
      "count.min_dwell_frames":    { "type": "integer", "min": 1,    "max": 60,  "default": 10 },
      "track.ttl_ms":              { "type": "integer", "min": 200,  "max": 3000,"default": 1200 },
      "zones.mask":                { "type": "object" },
      "zones.gain":                { "type": "object" },
      "fusion.lux_guard":          { "type": "integer", "min": 0,    "max": 3,   "default": 2 }
    }
  }
}
```

### 3.5 ACK / ERR

**ACK**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:41:00Z",
  "device_id": "radar-01",
  "seq": 302,
  "type": "ack",
  "payload": { "txid": "xyz", "ok": true }
}
```

**ERR**
```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:41:01Z",
  "device_id": "radar-01",
  "seq": 303,
  "type": "err",
  "payload": { "txid": "xyz", "code": "bad_path", "msg": "unknown config key" }
}
```

### 3.6 Comando IN (`type="cmd"`)

Enviado pelo integrador via MQTT (`.../cmd`) ou HTTP (`POST /v1/cmd`).  
Mesma estrutura do envelope; **entra**, não sai.

```jsonc
{
  "v": 1,
  "ts": "2025-10-14T14:41:00Z",
  "device_id": "radar-01",
  "seq": 301,
  "type": "cmd",
  "payload": { "op": "set", "path": "use_case.apply", "value": "sala_reuniao", "txid": "uc1" }
}
```

---

## 4. Semântica & Regras

- **Idempotência:** repetir `cmd` com o mesmo `txid` deve gerar o mesmo `ack`.  
- **Compatibilidade:** novos campos **opcionais**; não remover/renomear campos existentes.  
- **Timezone:** sempre UTC (`ts`).  
- **QoS sugerido:** `events` QoS 1; `meas` QoS 0; `status/cap` com `retain`.  
- **IDs de track:** técnicos, não persistem entre boots (sem PII).

---

## 5. Mini-OpenAPI (HTTP local)

```yaml
openapi: 3.0.3
info: { title: SenseGrid Local API, version: 0.1 }
paths:
  /v1/occupancy:
    get:
      summary: Ocupação atual agregada
      responses:
        "200":
          description: OK
          content:
            application/json:
              schema:
                type: object
                properties:
                  count: { type: integer }
                  confidence: { type: number }
                  zones: { type: object, additionalProperties: { type: number } }
  /v1/tracks:
    get:
      summary: Tracks ativos
      responses: { "200": { description: OK } }
  /v1/cmd:
    post:
      summary: Envia comando (mesmo payload do MQTT cmd)
      requestBody:
        required: true
        content:
          application/json: { schema: { type: object } }
      responses: { "200": { description: OK } }
  /v1/health:
    get: { summary: Status do dispositivo, responses: { "200": { description: OK } } }
```

---

## 6. Changelog

- **v0.1 (14/10/2025):** envelope unificado; tipos `meas/event/status/cap/ack/err/cmd`; tópicos MQTT/rotas HTTP; exemplos; schema inicial de config; KPIs definidos.
