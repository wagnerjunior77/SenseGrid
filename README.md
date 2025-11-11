# SenseGrid — Firmware de Ocupação por Radar (ESP32-C3 + ME73MS01)

Sistema de **detecção de presença/ocupação** usando um módulo de radar mmWave da família **ME73MS01** conectado a um **ESP32-C3**.  
O firmware fornece:
- **Estados de presença** (`none`, `exist`, `move`)
- **Distância estimada**, **SNR** e **intensidade de sinal**
- **Streaming em JSON** via Serial para depuração e coleta de dados
- **CLI de diagnóstico** (help, info, stream on/off, rate, json, range)

> **Use cases**: contador de presença em cômodos, “ocupado/livre” de ambientes, automação residencial, análise de ocupação em tempo real.

---

## Placa, fiação e energia

### Board
- **Alvo**: ESP32-C3 (ex.: *ESP32-C3 DevKitM-1* / “Generic ESP32-C3” no Arduino)
- **Core Arduino**: `esp32:esp32@3.3.2`

### Radar (ME73MS01 — família)
- Interface **UART** (3V3) + saída digital de **OCC** (ocupação).
- **Alimentação**: **verifique o seu módulo**. Existem variantes que aceitam 5 V e outras 3V3.  
  No nosso setup de teste: ESP32-C3 alimentado via USB; o radar foi alimentado conforme a especificação do módulo (atenção à compatibilidade de níveis).

### Pinos (padrão do projeto)
| Sinal         | ESP32-C3 | Observação                          |
|---------------|----------|-------------------------------------|
| UART1 RX      | **GPIO1**| Recebe do TX do radar               |
| UART1 TX      | **GPIO3**| Envia para RX do radar              |
| OCC (presença)| **GPIO4**| Entrada digital (pull-down interno) |
| GND           | GND      | Referência comum                    |
| VCC (radar)   | 3V3/5V*  | *Conforme sua versão do módulo      |

> **Atenção**: mantenha GND comum. **Não** conecte TX/RX sem GND compartilhado.

---

## Build e Upload (Arduino CLI portátil incluso no repo)

Requisitos:
- Windows + VS Code (Tasks já configuradas)
- Nada global: usamos `toolchain/arduino-cli.exe` e `toolchain/arduino-cli.yaml` (portátil)

### Passo a passo (VS Code → Terminal → “Run Task…”)

1. **Arduino (portable): Bootstrap CLI (1x)**  
   Baixa/garante o `arduino-cli.exe` em `toolchain/`.

2. **Arduino (portable): Configurar timeout (1x)**  
   Seta `network.connection_timeout=1200s` no YAML do repo.

3. **Arduino (portable): Update Index**

4. **Arduino (portable): Install Core (1x ou quando trocar versão)**  
   Instala `esp32:esp32@3.3.2`.

5. **Arduino (portable): Build + Export (bin/elf)**  
   Gera artefatos em `toolchain/build/SenseGrid/`:
   - `SenseGrid.ino.bin`, `SenseGrid.ino.elf` (e demais imagens quando aplicável)

6. **Arduino (portable): Upload (from exported binaries)**  
   Faz o flash usando os binários exportados.  
   > Configure a porta em `settings.json`: `"sensegrid.serialPort": "COM5"`

7. **Arduino (portable): Monitor**  
   Abre o serial monitor a **115200**.

### Configurações do workspace (já no repo)

`settings.json` (trecho relevante):
```json
{
  "sensegrid.fqbn": "esp32:esp32:esp32c3",
  "sensegrid.serialPort": "COM5",
  "sensegrid.coreVersion": "3.3.2"
}
```

---

## CLI rápida (diagnóstico)

Digite comandos no Serial (115200). Exemplos:

```text
help
info
stream on         # começa a imprimir JSON contínuo
stream off
json on           # força formato JSON (ao invés de logs humanos)
json off
rate 100          # 100 ms entre amostras (~10 Hz)
log info          # níveis: error, warn, info, debug
range 200         # limite de distância do radar em cm (ex.: 200 = 2 m)
```

> Dica: `stream on` + `json on` é o combo para gravar dados com um script no PC.

---

## Exemplo de saída JSON (real)

```json
{"ts_ms":147932,"status":"move","dist_m":0.730,"speed_mps":0.000,"snr":1.000,"distance_cm":73,"speed_cms":0,"signal":310}
{"ts_ms":148282,"status":"exist","dist_m":0.280,"speed_mps":0.000,"snr":1.000,"distance_cm":28,"speed_cms":0,"signal":1685}
{"ts_ms":42210,"status":"none","dist_m":0.000,"speed_mps":0.000,"snr":0.000,"distance_cm":0,"speed_cms":0,"signal":0}
```

Campos:
- **status**: `none` (vazio), `exist` (presença estática), `move` (movimento)
- **dist_m / distance_cm**: distância estimada
- **speed_mps / speed_cms**: velocidade (quando disponível)
- **snr**: razão sinal-ruído normalizada (0…1)
- **signal**: intensidade “bruta” do fabricante

---

## Range (limite de distância)

- O firmware aplica um **limite de presença** (ex.: **2 m → `range 200`**).  
- Para testar outros valores em runtime: `range <cm>` (ex.: `range 500` ≈ 5 m).
- A configuração enviada no boot também define **VO hold** (ex.: 3 s) e o limite de presença.

> Observação: a persistência de ajustes depende do comando “save” do módulo. O boot já envia um “salvar tudo” após aplicar os parâmetros padrão do projeto.

---

## Coleta de dados no PC (JSONL)

Use o script `tools/serial_logger.py` para salvar JSONL contínuo.  
Exemplo de uso:
```bash
python tools/serial_logger.py --port COM5 --baud 115200 --out logs/session.jsonl
```

---

## Estrutura do repo (essencial)

```
/components
  /cli
  /drivers
  /hal
  /util
/docs
  diagnostico.md
  radar_proto.md
/logs
/sketch
  /SenseGrid
    SenseGrid.ino
/toolchain
  arduino-cli.exe
  arduino-cli.yaml
  build-arduino.ps1
  ...
README.md
```

---

## Licença
Definir conforme necessidade do projeto (ex.: MIT). Créditos ao time SenseGrid e à Dyona (contexto do projeto).
