# Diagnóstico, CLI e Exemplos

Este documento consolida os **comandos da CLI** via Serial e exemplos reais de saída/uso.

---

## Console Serial (115200)

Comandos disponíveis:

```
help
info
stream on           # habilita streaming contínuo
stream off
json on             # força formato JSON limpo (1 linha por leitura)
json off
rate <ms>           # ex.: rate 100 → ~10 Hz (default típico)
log <level>         # error | warn | info | debug
range <cm>          # ex.: range 200 → 2 m (limite de presença)
```

**Dicas**
- `stream on` + `json on` = melhor modo para scripts/ingestão.
- `rate 0` → usa cadência interna do radar (sem imposição de período).
- `log debug` ajuda a investigar framing/tempo; `log info` é o padrão “limpo”.

---

## Exemplos reais

### 1) JSON de telemetria (stream)

```json
{"ts_ms":38858,"status":"move","dist_m":1.910,"speed_mps":0.000,"snr":0.422,"distance_cm":191,"speed_cms":0,"signal":863,"az_deg":0,"el_deg":0}
{"ts_ms":39107,"status":"none","dist_m":0.000,"speed_mps":0.000,"snr":0.000,"distance_cm":0,"speed_cms":0,"signal":0,"az_deg":0,"el_deg":0}
{"ts_ms":45753,"status":"move","dist_m":1.950,"speed_mps":0.000,"snr":1.000,"distance_cm":195,"speed_cms":0,"signal":2438,"az_deg":0,"el_deg":0}
```

- **status** alterna entre `move` (movimento), `exist` (parado com respiracao/postura sutil) e `none` (vazio).
- **dist_m** estabiliza proximo do alvo; fora do alcance configurado, retorna `none`.

### 2) Logs de OCC (pino digital do módulo)

```json
{"raw": "[INFO] [OCC] 0  t=994299"}
{"raw": "[INFO] [OCC] 1  t=995325"}
```

- **OCC=1** indica presença detectada pelo pino do radar (fallback simples).
- O firmware usa OCC como “sombra” quando o feed está “stale”.

### 3) Dump RAW (hex) do frame (modo debug)

Exemplo visto no monitor:
```
[RAW] 55 A5 00 0E 03 81 00 00 01 00 91 00 00 00 00 01 A5 C4 55 A5 ...
```
- Útil para validar `LEN` e `SUM` durante testes de framing.

---

## Critérios de teste (A2)

- **Hello-radar (10 min)**: 0 frames com checksum inválido; jitter de `ts_ms` < 5 ms.
- **Sanidade saturação/ausência**: mão muito próxima → distância pequena; ambiente vazio → `none` estável (sem flicker).
- **Dump .jsonl**: arquivo cresce continuamente com *1 JSON por linha*.

---

## Troubleshooting

- **Sem leitura**: verifique **RX/TX** cruzados (ESP RX ↔ TX do radar) e **GND comum**.
- **Alcance maior que o esperado**: confira `range <cm>` (ex.: `range 200`), e barreiras/parede atrás do alvo (radar “vaza” por materiais finos).
- **Falsos `exist`**: ajuste posição/ângulo do sensor; evite apontar para ventiladores/cortinas.
- **Mismatch de baud**: confirme `115200 8N1` (default do projeto).
