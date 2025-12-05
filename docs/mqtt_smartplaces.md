# Documentacao MQTT - Dyona Smartplaces

## Indice
- Estrutura de topicos MQTT
- Campos gerais dos dispositivos (info, status, OTA)
- Payloads e formatos por topico (meta, status, cfg outputs/inputs, estados)
- Exemplo de publicacao de estados
- Notas finais
- Smart PLC REST API (envelope e endpoints)

---

## Estrutura de topicos MQTT

Abreviacoes: sp = Smartplace; dt = Digital Twin (Digital do Device)

```
sp[smartbuilding_reference]/[deviceId]/c                     # comandos para o device
sp[smartbuilding_reference]/[deviceId]/dt/cfg                # configuracao geral (JSON)
sp[smartbuilding_reference]/[deviceId]/dt/meta               # informacoes do device
sp[smartbuilding_reference]/[deviceId]/dt/st                 # status do device (online/offline/LWT)
sp[smartbuilding_reference]/[deviceId]/dt/o/out/output_1     # estado de output individual
sp[smartbuilding_reference]/[deviceId]/dt/o/in/input_1       # estado de input individual
sp[smartbuilding_reference]/[deviceId]/dt/cfg/out/output_1   # configuracao do output
sp[smartbuilding_reference]/[deviceId]/dt/cfg/in/input_1     # configuracao do input
sp[smartbuilding_reference]/[deviceId]/dt/r/out/output_1     # rotinas programadas para o output
```

---

## Campos gerais dos dispositivos

### Info
| Campo          | Tipo   | Descricao                                          |
| -------------- | ------ | -------------------------------------------------- |
| device_id      | string | ID unico do dispositivo (serial ou chip ID).       |
| device_model   | string | Modelo logico/fisico do PLC.                       |
| provisioned_at | int    | Timestamp do provisionamento (epoch).              |
| version        | string | Versao do firmware instalada.                      |
| last_update    | int    | Timestamp da ultima atualizacao (epoch).           |
| ota_enabled    | bool   | Indica se ha fluxo OTA disponivel.                 |

### OTA
| Campo      | Tipo   | Descricao                                                                                                  |
| ---------- | ------ | ---------------------------------------------------------------------------------------------------------- |
| ota_status | string | disabled, idle, checking, downloading, preparing, applying, success, failed                                |
| last_check | int    | Timestamp da ultima verificacao (epoch).                                                                   |

### Status
| Campo            | Tipo | Descricao                                    |
| ---------------- | ---- | -------------------------------------------- |
| device_online    | bool | Indica se esta conectado a nuvem.            |
| last_time_online | int  | Timestamp da ultima conexao bem-sucedida.    |

---

## Payloads e formatos por topico

### Info do Dispositivo
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/meta`
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

### Status do Dispositivo
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/st`
```json
{
  "device_online": true,
  "last_time_online": 1721321231
}
```

### Status OTA
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/ota`
```json
{
  "ota_status": "downloading",
  "last_check": 1721321231
}
```

### Configuracao dos Outputs
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/cfg/out/output_1`
```json
{
  "meta": {
    "hw_label": "Saida 1 - Rele",
    "type": "0x01"
  },
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

Campos:
- meta: info fixa do HW (hw_label, type)
- settings: modo e parametros (mode 0x01 switch, 0x02 pulso; control_value; pulse_time ms; boot_behavior off/on/last)
- digital_child: ligacao logica (digital_device_id, output_function 0x01=toggle etc.)

### Configuracao dos Inputs
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/cfg/in/input_1`
```json
{
  "meta": {
    "hw_label": "Entrada 1 - Pulso",
    "type": "0x01"
  },
  "settings": {
    "mode": "0x01"
  },
  "targets": {
    "target_1": {
      "type": "0x10",
      "destination": "self:output_1"
    }
  }
}
```

Campos:
- meta: info fixa do HW (hw_label, type)
- settings: mode 0x01 switch, 0x02 pulso
- targets: mapa de alvos; cada target_n tem type (0x10 local, 0x20 HTTP etc.) e destination (self:output_X, device:<id>:output_X, ...)

---

## Exemplo de publicacao de estados

Estado de Output  
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/o/out/output_1`
```json
{ "state": true }
```

Estado de Input  
Topico: `sp[smartbuilding_reference]/[deviceId]/dt/o/in/input_1`
```json
{ "state": false }
```

---

## Notas Finais
- Multiplo output/input por device segue mesmo padrao de topico/payload.
- hw_label e nomes fixos de hardware; pode extender payload com campos novos (ex.: custom_name).
- Comandos para o device: `sp[smartbuilding_reference]/[deviceId]/c` (protocolo de comando ainda a definir).
- Last updated: 2025-08-19.

---

# Smart PLC REST API (resumo)

Envelope universal:
```json
{ "m": "<METHOD>", "e": "<endpoint>", "b": { ... } }
```
`m`: GET/POST/PATCH, `e`: caminho (ex.: /outputs/output_1), `b`: corpo.
IDs de canal e modos sao bytes representados como hex string (0xNN) no JSON.

## Outputs
- GET /outputs/output_{n}: retorna type, state, mode, pulse_time, boot_behavior.
- POST /outputs: body { k, c, v? } com comandos:
  - 0x00 off, 0x01 on, 0x02 toggle, 0x03 pulse (v=ms), 0x04 default action, 0xFF clear config.
- PATCH /outputs: body { k, state?, mode?, pulse_time?, boot_behavior? } (sinonimos pulse/pulse_time, output_mode/mode).

## Inputs
- GET /inputs/input_{n}: type, state, mode.
- POST /inputs: body { k, c: "0xFF" } para limpar config.
- PATCH /inputs: body { k, mode?, targets? }; targets ate 3 entradas target_1..target_3 com type e destination.

## Automacao
- POST /routine: body { k, cid, op (add|remove|clear|list), entries:[{"hex": ...}] }; resposta {"status":"accepted"}.
- POST /timer ou /automation/timer: agenda timer one-shot. Body { cid, output_key?, c, v, tm:ct|at, tv, policy:skip|fire_immediately|reschedule }. Resposta {"status":"scheduled"} ou {"status":"ignored"} se vencido.

## OTA
- POST /ota: body opcional manifest_url; resposta {"status":"started"}.

## Erros
- Qualquer endpoint/metodo/payload invalido: `{ "error": "<msg>" }`.
