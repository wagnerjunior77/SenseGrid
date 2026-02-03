# SenseGrid - AGENTS.md (for automations & new contributors)

> Purpose. Passo-a-passo objetivo para agentes (e humanos) trabalharem neste repo sem quebrar a arquitetura.
> Pilares: SOLID (com foco em DIP), endpoint-agnostic (Core desacoplado), build Arduino CLI portable.
> Fase atual: Atividade 8 - Testes controlados multiambiente (A/B sem PIR).

---

## 0) Visao rapida

- Alvo: ESP32-C3 DevKitC (esp32:esp32:esp32c3)
- Sensor principal: radar 24 GHz ME73MS01 via UART
- Arquitetura: components/ (C) contem Core + Drivers + Parser + Pipeline + Config + Serializer
- Sketch (C++) apenas integra/cola (glue) e expone CLI/monitor + HTTP/MQTT
- Regra de ouro: Core nao conhece endpoints. Todo IO externo passa por Adapters/Services.

---

## 1) Build oficial (Arduino CLI portable)

Nao use IDF aqui. As Tasks do VS Code tornam o build reproduzivel (sem instalacoes globais).

1. Arduino (portable): Bootstrap CLI (1x)
2. Arduino (portable): Configurar timeout (1x)
3. Arduino (portable): Prefetch discovery tools
4. Arduino (portable): Update Index
5. Arduino (portable): Install Core (1x ou quando trocar versao)
6. Arduino (portable): Build + Export (bin/elf)

Config padrao versionada (.vscode/settings.json):
- sensegrid.fqbn = esp32:esp32:esp32c3
- sensegrid.coreVersion = 3.3.2
- sensegrid.serialPort = AUTO (auto-detect)

Saida esperada: artefatos em toolchain/build/SenseGrid/

Driver USB (Windows): se a porta COM nao aparecer, rode Arduino (portable): Ensure CP210x driver (Win)

---

## 2) Upload e Monitor

- Upload (usando os binarios exportados):
  VS Code -> Run Task -> Arduino (portable): Upload (from exported binaries)

- Monitor Serial (115200):
  toolchain\arduino-cli.exe monitor `
    --config-file toolchain\arduino-cli.yaml `
    -p COM5 -c baudrate=115200
  Troque COM5 se necessario (ou defina SENSEGRID_PORT). As tasks de Upload/Monitor auto-detectam a porta.

---

## 3) CLI (Serial) - comandos suportados

help
info
stream on|off
rate <Hz>
json on|off
log <0..3>
pipe show / pipe set <key> <value>
wifi show
wifi set <ssid> <pass>

Formato JSON (stream): por linha, com chaves como ts_ms, status, dist_m, speed_mps, snr, distance_cm, speed_cms, signal.
Estados do radar (bruto): none, exist, move

---

## 4) Endpoints HTTP

- GET /v1/meas
- GET /v1/kpi
- GET /v1/diagnostics/status
- GET /v1/diagnostics/kpi
- GET /v1/diagnostics/export (jsonl)
- GET /diagnostics (pagina web)

Observacao: /v1/diagnostics/* sao aliases de diagnostico. /diagnostics exibe cards.

---

## 5) MQTT (telemetria + diagnostico)

Publica telemetria e KPI. Topicos seguem o padrao sp/<device_id>/... (ver docs/observabilidade.md).
KPI e publish (nao eh sub). Comandos de config ficam no CLI/HTTP.

---

## 6) Logger estruturado .jsonl

- Implementado em sketch/SenseGrid/sg_adapters_logger.*
- Grava jsonl em SPIFFS com rotacao e export via /v1/diagnostics/export
- Throttle de escrita para nao travar serial

---

## 7) Configuracao de rede

- sg_net (components/net) cuida de AP/STA, status e provisionamento
- Sem SSID/senha hard-coded
- CLI: wifi set <ssid> <pass>
- Logs nao exibem SSID em producao

---

## 8) Radar/Drivers

- components/radar/sg_radar.h + sg_radar_types.h definem interface neutra
- drivers/drv_radar_me73.* implementa driver e parse do frame
- Core depende apenas de sg_radar_types.h (DIP ok)

---

## 9) Core / Pipeline / Calibracao

- core/sg_core.*: aplica gating/range, snapshot atual, parametros, KPIs por janela
- pipeline/sg_pipe.*: filtros + state machine
- pipeline/sg_calib.*: assistente de calibracao
- Core nao conhece HTTP/MQTT/Serial

---

## 10) Sketch (SenseGrid.ino)

Responsavel por:
- boot: Serial, net, radar, core, HTTP, MQTT, CLI
- loop: leitura do radar -> core_step -> publish/stream
- sem logica de negocio (fica em components/)

---

## 11) Build notes importantes (novo)

- toolchain/include-dirs.txt agora deve conter apenas pastas de include existentes
- toolchain/build-arduino.ps1 ignora include inexistente
- build-arduino.ps1 cria link (junction) para components/ dentro do build path,
  pois glue inclui headers por caminho relativo (../../../components/...)

Se aparecer erro de "No such file or directory" em glue, confirme:
- build-arduino.ps1 atualizado
- pasta toolchain/build/SenseGrid/components existe (junction)

---

## 12) Estado atual (A8)

- Observabilidade concluida (KPI, HTTP diagnostics, MQTT KPI, logger jsonl, dashboard)
- Heatmap e zoneamento descartados (sensor nao fornece angulos validos)
- Atividade 8: testes controlados multiambiente (A/B sem PIR)

---

## 13) Troubleshooting rapido

- Checksum invalido: ruido UART ou baud incorreto. Garanta GND comum e fios curtos.
- Porta COM ocupada: feche monitores/serial anteriores.
- Build lento/falha de rede: rode Configurar timeout (1x).
- Compila mas nao acha headers: ver item 11.

---

## 14) Estilo de PR

- PRs pequenos e incrementais
- Se alterar contrato/JSON, atualize docs/ e exemplos
- Nao introduza dependencia de endpoint no core/
- Anexe amostras reais (.jsonl) quando mexer em parser/CLI

---

## 15) Roadmap (curto)

- A8: testes A/B multiambiente sem PIR (baseline e comparacao de kpi)
- A9: ajustes finos de pipeline (gating/histerese) se necessario