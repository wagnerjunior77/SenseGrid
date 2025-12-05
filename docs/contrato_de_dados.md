# Contrato de Dados - Dyona Smartplaces (SenseGrid)

**Versao:** v0.2 (alinhado ao doc MQTT Dyona - 2025-08-19)  
**Compatibilidade:** novas adicoes devem ser opcionais (nao quebrar campos existentes).

## Identificadores e topicos
- `smartbuilding_reference`: codigo do smartplace (configuracao local/NVS).
- `deviceId`: identificador unico do device (serial/chip-id/config).

Padrao de topicos MQTT:
```
sp[smartbuilding_reference]/[deviceId]/c                     # comandos para o device
sp[smartbuilding_reference]/[deviceId]/dt/cfg                # configuracao geral (JSON)
sp[smartbuilding_reference]/[deviceId]/dt/meta               # informacoes do device
sp[smartbuilding_reference]/[deviceId]/dt/st                 # status (LWT)
sp[smartbuilding_reference]/[deviceId]/dt/ota                # status OTA
sp[smartbuilding_reference]/[deviceId]/dt/o/out/output_1     # estado de output
sp[smartbuilding_reference]/[deviceId]/dt/o/in/input_1       # estado de input
sp[smartbuilding_reference]/[deviceId]/dt/cfg/out/output_1   # configuracao de output
sp[smartbuilding_reference]/[deviceId]/dt/cfg/in/input_1     # configuracao de input
sp[smartbuilding_reference]/[deviceId]/dt/r/out/output_1     # rotinas do output
```

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
- Topico `.../c`: protocolo a definir pelo device (pode usar o envelope REST abaixo ou comandos especificos).

## REST local (resumo)
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
