# SenseGrid — AGENTS.md (for automations & new contributors)

> **Purpose.** Passo-a-passo objetivo para agentes (e humanos) trabalharem neste repo **sem quebrar a arquitetura**.  
> **Pilares:** SOLID (com foco em DIP), **endpoint-agnostic** (Core desacoplado), build **Arduino CLI portable**.  
> **Fase atual:** **Atividade 2 — Bring-up de hardware & drivers** (A3 virá na próxima etapa).

---

## 0) Visão rápida

- **Alvo:** ESP32-C3 DevKitC (`esp32:esp32:esp32c3`).  
- **Sensor principal:** radar 24 GHz ME73MS01 via **UART**.  
- **Arquitetura:** `components/` (C) contém **Core** + **Drivers** + **Parser**. O **sketch** (C++) apenas integra/cola (glue) e expõe CLI/monitor.  
- **Regra de ouro:** **Core não conhece endpoints**. Qualquer IO externo (HTTP/MQTT/Flash/Serial logs) passa por **Adapters**.

---

## 1) Build oficial (Arduino CLI portable)

> Não use IDF aqui. As *Tasks* do VS Code tornam o build reprodutível (sem instalações globais).

1. **Baixar CLI (1x)**  
   VS Code → **Run Task** → `Arduino (portable): Bootstrap CLI (1x)`

2. **Ajustar timeout (1x)**  
   VS Code → **Run Task** → `Arduino (portable): Configurar timeout (1x)`  
   (isso seta `network.connection_timeout=1200s` no `toolchain/arduino-cli.yaml`).

3. **Preencher cache de discovery (1x)**  
   VS Code → **Run Task** → `Arduino (portable): Prefetch discovery tools`  
   (garante `mdns-discovery` local; evita travar no update-index).

4. **Atualizar índice & instalar core (1x ou quando trocar versão)**  
   VS Code → **Run Task** → `Arduino (portable): Update Index`  
   VS Code → **Run Task** → `Arduino (portable): Install Core (1x ou quando trocar versão)`

5. **Build + export `.bin/.elf`**  
   VS Code → **Run Task** → `Arduino (portable): Build + Export (bin/elf)`

**Config padrão versionada** (`.vscode/settings.json`):  
- `sensegrid.fqbn = esp32:esp32:esp32c3`  
- `sensegrid.coreVersion = 3.3.2`  
- `sensegrid.serialPort = COM5`

**Saída esperada:** artefatos em `toolchain/build/SenseGrid/`.

**Driver USB (Windows):** se a porta COM não aparecer, rode `Arduino (portable): Ensure CP210x driver (Win)` (usa o pacote `driver/` com pnputil; pode pedir admin). As tasks de Upload/Monitor já dependem dela.

---

## 2) Upload e Monitor

- **Upload** (usando os binários exportados):  
  VS Code → **Run Task** → `Arduino (portable): Upload (from exported binaries)`

- **Monitor Serial** (115200):  
  ```powershell
  toolchain\arduino-cli.exe monitor `
    --config-file toolchain\arduino-cli.yaml `
    -p COM5 -c baudrate=115200
Troque COM5 se necessário. O repo assume COM5 por padrão.

3) CLI (Serial) — comandos suportados
pgsql
Copiar código
help               # lista os comandos
info               # info de build/pinos e range atual
stream on|off      # liga/desliga streaming contínuo
rate <Hz>          # limita a taxa do stream (0 = sem limite)
json on|off        # seleciona JSON por linha ou formato humano
log <0..3>         # nível: 0=error, 1=warn, 2=info (padrão), 3=debug
# (quando habilitado) pipe show / pipe set <key> <value>
Formato JSON (stream): por linha, com chaves como ts_ms, status, dist_m, speed_mps, snr, distance_cm, speed_cms, signal.
Estados do radar (bruto): none, exist, move (podem ser filtrados no pipeline A3).

4) Logger de sessão (PC)
Script simples para salvar .jsonl com tudo que chega na serial:

bash
Copiar código
python tools/serial_logger.py -p COM5 -b 115200 -o logs/session.jsonl
Gera 1 JSON por linha (fácil de abrir no pandas/Excel/BigQuery).

Use --raw para registrar também linhas não-JSON (ex.: logs de boot).

5) Conexões / Pinos (checar no código)
Radar ME73MS01 (UART)

Alimente conforme o módulo (datasheet). Muitos módulos aceitam 3.6–5.5 V; nível lógico UART é 3V3.

UART do ESP32-C3: usar o par RX/TX definido em sketch/SenseGrid/pins_radar.h

SG_RADAR_RX = RX do ESP (liga no T do radar)

SG_RADAR_TX = TX do ESP (liga no R do radar)

OCC (digital) → SG_PIN_RADAR_OCC (pull adequado no código).

Os nomes e GPIOs estão no código (arquivo pins_radar.h). Se alterar fios, altere lá e commite.

6) Configuração do radar no boot (range fixo 2,00 m)
No radar_config_boot() são enviados frames proprietários do ME73MS01:

VO hold = 3000 ms (0x0BB8): 55 5A 00 06 01 80 14 0B B8 0D

Presence max = 200 cm (0x00C8): 55 5A 00 06 01 80 0E 00 C8 0C

Save all: 55 5A 00 04 01 20 04 D8

Esses bytes incluem header, comprimento, comando, payload e checksum. Para outro alcance, alterar o payload (ex.: 500 cm → 0x01F4) e recomputar o checksum.

7) Regras de arquitetura (fiscais do SOLID)
DIP: core/ fala apenas com interfaces em components/*/include.

Drivers: acesso a HW/barramentos (UART/I2C/GPIO). Sem lógica de negócios.

Parser: bytes → medidas (distance_cm, speed_cms, signal, snr, status).

Core: agrega/filtra/aplica zona/presets. Não publica MQTT/HTTP/Serial diretamente.

Adapters: HTTP/MQTT/Logger/FlashRepo/Ports — todo IO externo sai por aqui.

Proibido no Core: WiFi.begin, HTTPServer, MQTTClient, Serial.printf (use adapter Logger).

8) Estado A2 (o que consideramos “pronto”)
✅ UART do radar inicializada; frames válidos por ≥10 min (0 checksum inválido).

✅ JSON estável com {distance, speed, signal, snr, status} no stream.

✅ CLI básica operacional (comandos acima).

✅ Build reprodutível e artefatos exportados.

⏭️ A3 (próximo): pipeline de detecção com gating por distância/SNR, filtros (median3 + IIR), baseline EMA e histerese/holds.

9) Troubleshooting rápido
Leitura “sempre move” / vibração: ventiladores e micro-movimento do suporte podem induzir falso move. Fixe o módulo (fita dupla face/abraçadeira) e reduza vibrações. A3 introduzirá histerese e gating para robustez.

Compila mas “não acha headers de components/”: confira #include relativos no glue e o includePath do VS Code.

Checksum inválido: ruído na UART ou baud incorreto. Garanta GND comum e fios curtos.

Porta COM ocupada: feche monitores/serial anteriores.

Build lento/falha de rede: rode a Task de Configurar timeout (1x).

10) Estilo de PR
PRs pequenos e incrementais; se alterar contrato/JSON, atualize docs/ e exemplos.

Não introduza dependência de endpoint no core/. Se precisar, crie/estenda um Adapter.

Anexe amostras reais (trechos .jsonl) quando tocar no parser/CLI.

11) Matriz “known-good”
Board: ESP32-C3 DevKitC

FQBN: esp32:esp32:esp32c3

Core: 3.3.2

Baud: 115200

Serial port: COM5 (ajustável)

12) Roadmap curto (A3)
components/pipeline/ com API:

c++
Copiar código
struct SgPipeIn  { uint8_t raw_status; uint16_t dist_cm; int16_t speed_cms; float snr; uint32_t now_ms; };
enum SgState { SG_EMPTY=0, SG_PRESENCE=1, SG_MOTION=2 };
struct SgPipeOut { SgState state; SgState stable; uint32_t stable_ms; bool gated; };
void   sg_pipe_init();
void   sg_pipe_set_params(const SgParams& p);
SgPipeOut sg_pipe_step(const SgPipeIn& in);
Gating por distância/SNR; filtros (median3 + IIR); baseline EMA; histerese + holds.

CLI: pipe show / pipe set <key> <value> para tunar parâmetros em tempo real.


