# Contrato de Dados - Dyona Smartplaces (SenseGrid)

**Versao:** v0.2 (alinhado ao doc MQTT Dyona - 2025-08-19)  
**Compatibilidade:** novas adicoes devem ser opcionais (nao quebrar campos existentes).

## Identificadores e envelope padrao
- `smartbuilding_reference`: codigo do smartplace (configuracao local/NVS).
- `deviceId`: identificador unico do device (serial/chip-id/config).
- Envelope (HTTP/MQTT/WS) sempre inclui: `v`, `ts_ms`, `ts_iso`, `device_id`, `seq`, `type`, `payload`.

Exemplo (occupancy):
```json
{"v":1,"ts_ms":47161,"ts_iso":"1970-01-01T00:00:47.161Z","device_id":"sg-90120803F784","seq":62,"type":"occupancy","payload":{"count":1,"confidence":0.900}}
```

## Topicos MQTT (prefixo `sp<sb>/<deviceId>/...`)
```
.../c                        # comandos para o device (JSON)
.../meas                     # medidas estabilizadas (distance/speed/signal + status)
.../meas_raw                 # medidas brutas (igual serial JSON, inclui az_deg/el_deg)
.../events                   # eventos de mudanca de estado (presence.changed)
.../status                   # status basico do device
.../cap                      # capacidades (sensores/eventos)
.../ack                      # ack de comandos (qos1)
.../err                      # erro de comandos (qos1)
.../dt/meta                  # info do device
.../dt/st                    # online/offline (pode ser LWT)
.../dt/ota                   # status OTA
.../dt/cfg/out/output_1      # configuracao de output
.../dt/cfg/in/input_1        # configuracao de input
.../dt/o/out/output_1        # estado de output
.../dt/o/in/input_1          # estado de input
.../dt/r/out/output_1        # rotinas programadas do output
```

## WebSocket (WS)
- Porta 81 (sem path dedicado), payload igual ao envelope HTTP (occupancy).
- Conectar em `ws://<ip>:81` e consumir JSON com `type":"occupancy"` e `payload{count,confidence}`.

## HTTP local (resumo)
- GET `/v1/occupancy` -> envelope + payload {count, confidence}
- GET `/v1/tracks` -> {active}
- GET `/v1/health` -> {fw, uptime_s, rssi_dbm}
- GET `/v1/meas` -> payload completo (dist_m, speed_mps, snr, distance_cm, speed_cms, signal, az_deg, el_deg, state, stable, stable_ms, in_range)
- GET `/v1/net` -> status da rede (sta_set, ap_set, sta_connected, sta_ip, ap_ip)
- POST `/v1/net` -> provisiona rede (sta_ssid, sta_pass, ap_ssid, ap_pass, clear)
- GET `/setup` -> portal HTML de provisionamento
- POST `/v1/cmd` -> placeholder (ack simples, nao executa comando hoje)

## Campos gerais
### Info (dt/meta)
```json
{
  "device_id": "ABC123456",
  "device_model": "DY-PLC-8R",
  "provisioned_at": 1712332123,
  "version": "1.0.5",
  "last_update": 1721321231,
  "ota_enabled": true
}
```

### Status (dt/st)
```json
{ "device_online": true, "last_time_online": 1721321231 }
```

### OTA (dt/ota)
```json
{ "ota_status": "downloading", "last_check": 1721321231 }
```
- ota_status: disabled | idle | checking | downloading | preparing | applying | success | failed

## Configuracao de outputs
Topico: `.../dt/cfg/out/output_1`
```json
{
  "meta": { "hw_label": "Saida 1 - Rele", "type": "0x01" },
  "settings": {
    "mode": "0x01",
    "control_value": "0x00",
    "pulse_time": 1000,
    "boot_behavior": "off"
  },
  "digital_child": {
    "digital_device_id": "lamp001",
    "output_function": "0x01"
  }
}
```
- type/mode/output_function sao hex strings (0xNN).
- mode: 0x01 switch, 0x02 pulso.
- boot_behavior: off | on | last.

## Configuracao de inputs
Topico: `.../dt/cfg/in/input_1`
```json
{
  "meta": { "hw_label": "Entrada 1 - Pulso", "type": "0x01" },
  "settings": { "mode": "0x01" },
  "targets": {
    "target_1": { "type": "0x10", "destination": "self:output_1" }
  }
}
```
- mode: 0x01 switch, 0x02 pulso.
- targets: type 0x10 local, 0x20 http, etc.; destination ex.: self:output_X ou device:<id>:output_X.

## Estados
- Output: topico `.../dt/o/out/output_1` payload `{ "state": true }`
- Input:  topico `.../dt/o/in/input_1`  payload `{ "state": false }`

## Comandos (MQTT)
- Topico `.../c`, payload JSON com `op`:
  - `set` path+value (ex.: pipe.dist_max)
  - `calib.start` dur_ms opcional
  - `calib.abort`
  - Resposta: `ack` ou `err` (contendo txid).

## REST local (opcional/legado)
Envelope universal:
```json
{ "m": "<METHOD>", "e": "<endpoint>", "b": { ... } }
```
- m: GET | POST | PATCH
- e: caminho (ex.: /outputs/output_1)
- b: corpo conforme endpoint

### Outputs
- GET /outputs/output_{n}: retorna type, state, mode, pulse_time, boot_behavior.
- POST /outputs: body { k, c, v? } comandos:
  - 0x00 off, 0x01 on, 0x02 toggle, 0x03 pulse (v=ms), 0x04 default, 0xFF clear.
- PATCH /outputs: body { k, state?, mode?, pulse_time?, boot_behavior? } (sinonimos: pulse/pulse_time, output_mode/mode).

### Inputs
- GET /inputs/input_{n}: type, state, mode.
- POST /inputs: body { k, c:"0xFF" } limpa config.
- PATCH /inputs: body { k, mode?, targets? } targets target_1..target_3 com type/destination.

### Rotinas e timer
- POST /routine: { k, cid, op:add|remove|clear|list, entries:[{"hex": "..."}] } -> {"status":"accepted"}
- POST /timer ou /automation/timer: { cid, output_key?, c, v, tm:ct|at, tv, policy:skip|fire_immediately|reschedule } -> {"status":"scheduled"} ou {"status":"ignored"}

### OTA
- POST /ota: { manifest_url? } -> {"status":"started"}

### Erros
- Qualquer erro: `{ "error": "<msg>" }`

## Notas
- Campos hex (0xNN) sao serializados como string para evitar re-interpretacao em decimal.
- hw_label e nomes fisicos sao fixos do hardware; payloads podem receber campos adicionais (ex.: custom_name).
- Multiplicar outputs/inputs basta replicar topicos/payloads por canal.
