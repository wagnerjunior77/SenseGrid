# SenseGrid Firmware (ESP32-C3 + Multi-Radar)

Firmware de presenca/ocupacao para ESP32-C3 com arquitetura modular.
O projeto suporta mais de um radar via interface comum (`SgRadarOps`).

## Estado atual

- Fase: Atividade 8 (testes multiambiente A/B sem PIR)
- Observabilidade concluida: KPI, endpoints de diagnostico, logger `.jsonl`, dashboard
- Build oficial: Arduino CLI portable via Tasks do VS Code

## Sensores suportados

- ME73MS01 (`components/drivers/drv_radar_me73.*`)
- LD2410C (`components/drivers/drv_radar_ld2410c.*`)

No boot, o firmware tenta detectar automaticamente o driver em runtime (LD2410C e ME73).

## Guia rapido

### 1) Build oficial (Arduino CLI portable)

Rode as Tasks na ordem abaixo (primeira vez no PC):

1. `Arduino (portable): Bootstrap CLI (1x)`
2. `Arduino (portable): Configurar timeout (1x)`
3. `Arduino (portable): Prefetch discovery tools`
4. `Arduino (portable): Update Index`
5. `Arduino (portable): Install Core (1x ou quando trocar versao)`
6. `Arduino (portable): Build + Export (bin/elf)`

Configuracao versionada em `.vscode/settings.json`:

- `sensegrid.fqbn = esp32:esp32:esp32c3`
- `sensegrid.coreVersion = 3.3.2`
- `sensegrid.serialPort = AUTO`

Saida esperada: `toolchain/build/SenseGrid/`.

### 2) Upload e monitor

1. `Arduino (portable): Ensure CP210x driver (Win)` (se a COM nao aparecer)
2. `Arduino (portable): Upload (from exported binaries)`
3. `Arduino (portable): Monitor`

As tasks de Upload/Monitor usam scripts com auto-detect de porta:

- porta `AUTO` por default
- override por `SENSEGRID_PORT`
- tenta filtrar por FQBN
- se houver mais de uma porta, prioriza CP210x (VID `0x10C4`)

## Referencia completa de tasks

### Setup e toolchain

- `Arduino (portable): Bootstrap CLI (1x)`
Baixa/garante `toolchain/arduino-cli.exe`.

- `Arduino (portable): Configurar timeout (1x)`
Define `network.connection_timeout=1200s` em `toolchain/arduino-cli.yaml`.

- `Arduino (portable): Prefetch discovery tools`
Preenche cache de discovery (evita travar no `update-index`).

- `Arduino (portable): Update Index`
Atualiza indice de cores do Arduino CLI.

- `Arduino (portable): Install Core (1x ou quando trocar versao)`
Instala `esp32:esp32@${config:sensegrid.coreVersion}`.

- `Arduino (portable): Prune Toolchain (optional)`
Remove ferramentas nao usadas pelo ESP32-C3 (`esp-x32`, `xtensa-esp-elf-gdb`, `esp-xs2`, `esp-xs3`) para reduzir uso de disco.

### Build e deploy

- `Arduino (portable): Build + Export (bin/elf)`
Compila e exporta artefatos para `toolchain/build/SenseGrid/`.

- `Arduino (portable): Ensure CP210x driver (Win)`
Instala driver USB-UART CP210x pelo pacote local.

- `Arduino (portable): Upload (from exported binaries)`
Faz flash usando `toolchain/upload-arduino.ps1` (com auto-detect de porta).

- `Arduino (portable): Monitor`
Abre serial monitor via `toolchain/monitor-arduino.ps1` (com auto-detect de porta).

### Tasks legadas

- `ESP-IDF (legacy): Build`
- `ESP-IDF (legacy): Flash`

Sao placeholders. O fluxo oficial deste repo e Arduino portable.

## CLI (serial)

```text
help
info
stream on|off
rate <ms>
json on|off
log error|warn|info|debug
pipe show
pipe set <key> <value>
range <cm|2|4|6>
calib start|status|apply|reset
raw on|off|once
mqtt show|host <ip>|port <n>|restart
wifi show|set <ssid> <pass>|ap <ssid> [pass]|clear [sta|ap|all]|apply
```

## Endpoints HTTP

- `GET /v1/meas`
- `GET /v1/kpi`
- `GET /v1/diagnostics/status`
- `GET /v1/diagnostics/kpi`
- `GET /v1/diagnostics/export` (jsonl)
- `GET /diagnostics` (UI local)
- `GET /v1/info`
- `GET /v1/net`
- `POST /v1/net`
- `GET /v1/pipe`
- `GET /v1/occupancy`
- `GET /v1/tracks`
- `GET /v1/health`

## MQTT (resumo)

Publica telemetria/KPI no padrao SmartPlaces (`sp<sb_ref>/<device_id>/...`).
Comandos de ajuste entram por topico de comando (`.../c`) e sao processados em `sketch/SenseGrid/mqtt_service.cpp`.

## Coleta de dados

- Sessao serial: `python tools/serial_logger.py -p COM5 -b 115200 -o logs/session.jsonl`
- Captura A/B: `python tools/ab_capture.py -p COM5 -b 115200 -d 120 -o logs/ab`

## Estrutura do codigo

Para onboarding tecnico e navegacao do repositorio:

- [docs/repository_structure.md](docs/repository_structure.md)

## Regras de arquitetura

- Core nao conhece endpoint.
- Drivers acessam HW (UART/GPIO).
- Parser converte bytes em medidas.
- Adapters/services fazem IO externo (HTTP/MQTT/log/rede).

Referencia operacional para agentes e contribuidores:

- [AGENTS.md](AGENTS.md)

## Troubleshooting rapido

- Erro de include em `glue/*`: confira `toolchain/build-arduino.ps1` e `toolchain/include-dirs.txt` atualizados.
- Sem porta serial: rode `Ensure CP210x driver`, ou force com `SENSEGRID_PORT`.
- Build pesado em disco: rode `Prune Toolchain (optional)` e limpe `toolchain/build/`.
- Checksum invalido: revisar GND comum, baud e cabos UART.
