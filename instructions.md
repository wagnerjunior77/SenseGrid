# SenseGrid — Instructions

**Resumo curto:** este repositório compila firmware para **ESP32-S3**. Vamos padronizar a build com **Arduino CLI** (portável, dentro do repo), porque o setup do ESP‑IDF puro variou demais entre máquinas (Python/constraints/env). O Arduino CLI compila em cima da IDF, então seguimos compatíveis com bibliotecas IDF quando necessário (ex.: NimBLE). Futuramente vamos acoplar OTA, MQTT, etc. via stack Arduino.

---

## 1) Contexto do projeto

**Projeto:** SenseGrid (Dyona).

**Pessoas chave:**

- **Paulo (Dyona):** sinalizou que “tanto faz” compilar via IDF puro ou Arduino CLI, porque o Arduino CLI roda em cima da IDF e as duas empilham juntas.
- Em builds anteriores houve **conflito de headers** (uma lib que o Arduino abstrai tinha headers que colidiam com os da IDF). A solução foi usar a lib da IDF com cabeçalhos do Arduino CLI, ajustando includes.
- **Serviços planejados (depois):** OTA, MQTT, etc. — stack Arduino CLI (bibliotecas Arduino), mantendo possibilidade de chamar APIs nativas IDF se necessário.

**Por que migramos de IDF puro para Arduino CLI?**

- Em várias máquinas houve erros com:
  - Python ausente/diferente, venv/virtualenv, pip indisponível.
  - *constraints* do IDF (5.5.x) faltando ou variando (rede/proxy).
  - Diferenças de shell (PowerShell × cmd) e *quoting*.
- O Arduino CLI **portável** resolve ao máximo essas variáveis: baixamos o `arduino-cli.exe` para `toolchain/` dentro do repo, com config YAML também no repo. Assim não exigimos instalações globais no PC do cliente.

---

## 2) Estratégia atual (oficial): Arduino CLI Portável

### 2.1 Estrutura esperada do repo

```
<sensegrid-root>/
  src/                     # seu código (padrão Arduino sketch-style ou .ino/.cpp)
  include/                 # headers do projeto
  platformio.ini?          # (se houver histórico; não é usado aqui)
  scripts/
    bootstrap.ps1          # (opcional/histórico IDF) - pode manter ou remover
  toolchain/
    arduino-cli.exe        # CLI baixado localmente (portável)
    arduino-cli.yaml       # config do CLI (em repo)
    arduino-data/          # caches/cores (gerado pelo CLI)
    arduino-downloads/     # downloads (gerado)
  .vscode/
    tasks.json             # Tasks VS Code (Arduino)
    settings.json          # Configs (FQBN/porta)
  instructions.md          # ESTE arquivo
```

> Importante: **nada** do Arduino precisa ser instalado globalmente. Tudo vive em `toolchain/`.

### 2.2 Configurar o Arduino CLI (1ª vez)

Você pode fazer manualmente (ou via Task **“Bootstrap CLI + Core”**, ver Seção 3.1):

1. **Baixe** o Arduino CLI para Windows x64 e salve como:
   `toolchain/arduino-cli.exe`

2. Crie `toolchain/arduino-cli.yaml` com conteúdo mínimo:

```yaml
board_manager:
  additional_urls:
    - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

directories:
  data: ./toolchain/arduino-data
  downloads: ./toolchain/arduino-downloads
  user: ./
```

> Paths relativos funcionam pois o CLI resolve a partir da pasta atual. Mantemos tudo **dentro do repo**.

3. Atualize índices e instale a core do ESP32:

```powershell
toolchain\arduino-cli.exe config dump --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe core update-index --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe core install esp32:esp32 --config-file toolchain\arduino-cli.yaml
```

4. *(Opcional)* Liste FQBNs (boards) e portas seriais:

```powershell
toolchain\arduino-cli.exe board listall --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe board list     --config-file toolchain\arduino-cli.yaml
```

### 2.3 FQBN (placa)

Usamos **ESP32-S3** (core Arduino‑ESP32 v3.x). FQBN típico:

```
esp32:esp32:esp32s3
```

Se precisar de variantes (USB CDC On Boot, PSRAM, clock, etc.), você pode inspecionar as opções com:

```powershell
toolchain\arduino-cli.exe board details --fqbn esp32:esp32:esp32s3 --config-file toolchain\arduino-cli.yaml
```

…e ajustar via `--build-property` (ver abaixo).

### 2.4 Compilar / Upload / Monitor (CLI cru)

```powershell
# Compilar (exemplo com flag NimBLE)
toolchain\arduino-cli.exe compile ^
  --config-file toolchain\arduino-cli.yaml ^
  --fqbn esp32:esp32:esp32s3 ^
  --build-property compiler.cpp.extra_flags="-DCONFIG_NIMBLE_CPP_IDF=1" ^
  .

# Upload (ajuste COMx)
toolchain\arduino-cli.exe upload ^
  --config-file toolchain\arduino-cli.yaml ^
  --fqbn esp32:esp32:esp32s3 ^
  -p COM4 ^
  .

# Monitor serial
toolchain\arduino-cli.exe monitor ^
  --config-file toolchain\arduino-cli.yaml ^
  -p COM4 ^
  -c baudrate=115200
```

> Observação (headers/lib IDF): quando for necessário usar libs nativas da IDF dentro do sketch, essa flag `-DCONFIG_NIMBLE_CPP_IDF=1` é um exemplo de ajuste que já usamos para orientar o NimBLE a usar a IDF “por baixo” mantendo compatibilidade com o stack Arduino.

---

## 3) VS Code — Tasks e Settings (Arduino)

### 3.1 `.vscode/tasks.json`

Use essas tasks (sem concatenar strings PowerShell com `+` para evitar parsing quebrado). Elas chamam o `arduino-cli.exe` do repositório:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Arduino (portable): Bootstrap CLI + Core (1x)",
      "detail": "Baixa/valida CLI local e instala esp32:esp32",
      "type": "shell",
      "options": { "cwd": "${workspaceFolder}" },
      "command": "${workspaceFolder}\\\\toolchain\\\\arduino-cli.exe",
      "args": [
        "core", "install", "esp32:esp32",
        "--config-file", "${workspaceFolder}\\\\toolchain\\\\arduino-cli.yaml"
      ],
      "problemMatcher": []
    },
    {
      "label": "Arduino (portable): Build",
      "type": "shell",
      "options": { "cwd": "${workspaceFolder}" },
      "command": "${workspaceFolder}\\\\toolchain\\\\arduino-cli.exe",
      "args": [
        "compile",
        "--config-file", "${workspaceFolder}\\\\toolchain\\\\arduino-cli.yaml",
        "--fqbn", "${config:sensegrid.fqbn}",
        "--build-property", "compiler.cpp.extra_flags=-DCONFIG_NIMBLE_CPP_IDF=1",
        "${workspaceFolder}"
      ],
      "problemMatcher": ["$gcc"],
      "dependsOn": ["Arduino (portable): Bootstrap CLI + Core (1x)"]
    },
    {
      "label": "Arduino (portable): Upload",
      "type": "shell",
      "options": { "cwd": "${workspaceFolder}" },
      "command": "${workspaceFolder}\\\\toolchain\\\\arduino-cli.exe",
      "args": [
        "upload",
        "--config-file", "${workspaceFolder}\\\\toolchain\\\\arduino-cli.yaml",
        "--fqbn", "${config:sensegrid.fqbn}",
        "-p", "${config:sensegrid.serialPort}",
        "${workspaceFolder}"
      ],
      "problemMatcher": []
    },
    {
      "label": "Arduino (portable): Monitor",
      "type": "shell",
      "options": { "cwd": "${workspaceFolder}" },
      "command": "${workspaceFolder}\\\\toolchain\\\\arduino-cli.exe",
      "args": [
        "monitor",
        "--config-file", "${workspaceFolder}\\\\toolchain\\\\arduino-cli.yaml",
        "-p", "${config:sensegrid.serialPort}",
        "-c", "baudrate=115200"
      ],
      "problemMatcher": []
    }
  ]
}
```

### 3.2 `.vscode/settings.json`

Defina a placa e a porta uma vez só:

```json
{
  "//": "Se quiser C/C++ IntelliSense pegando compile_commands.json, gere via CLI em modo verbose/simbolico",
  "C_Cpp.intelliSenseEngine": "default",

  "sensegrid.fqbn": "esp32:esp32:esp32s3",
  "sensegrid.serialPort": "COM4"
}
```

> Altere `COM4` para a porta correta do seu PC. Cheque com **Arduino (portable): Monitor** ou `board list`.

---

## 4) Como o Arduino CLI “roda em cima” da IDF

A core `esp32:esp32` embute uma determinada versão da IDF. Ao compilar, o CLI invoca a toolchain/IDF interna da core.

Por isso é possível:

- Incluir headers IDF (`esp_system.h`, etc.).
- Ajustar flags para escolher implementações (como NimBLE/IDF).

**Benefício:** menos fricção de ambiente (Python, constraints, export.bat, etc.), mantendo acesso ao “mundo IDF”.

---

## 5) Roadmap técnico (curto)

1. **Consolidar build** por Arduino CLI (feito aqui).
2. Integrar **OTA** (*ArduinoOTA* ou lib custom) — padronizar credenciais e estratégia de segurança/atualização.
3. Integrar **MQTT** (ex.: *AsyncMqttClient*/*PubSubClient* ou lib interna) — padronizar tópicos, QoS, resiliência.
4. **Telemetria/diagnóstico** (logs, reset reasons).
5. **Refinar particionamento** se necessário (pode ser ajustado via `--build-property`, ver seção adicional abaixo).

---

## 6) Dicas & Troubleshooting

- **Drivers USB (Windows):** se não abrir porta COM, instale drivers da sua placa (CP210x, CH340, FTDI, etc.).
- **Permissões:** execute VS Code “como Administrador” só se necessário para portas/driver.
- **Baixar core falhou:** verifique conexão/SSL/Firewall/Proxy; o índice está em  
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
- **Variantes/partições:**  
  Você pode passar propriedades extras:
  ```powershell
  --build-property build.partitions=huge_app
  --build-property upload.maximum_size=...  # (cuidado)
  ```
  Descubra chaves disponíveis com:
  ```powershell
  toolchain\arduino-cli.exe board details --fqbn esp32:esp32:esp32s3 --config-file toolchain\arduino-cli.yaml
  ```
- **NimBLE/IDF:** se precisar desambiguar includes, mantenha `-DCONFIG_NIMBLE_CPP_IDF=1` e organize `#include` (Arduino vs IDF).
- **Monitor trava após upload:** às vezes ajuda definir `--fqbn` com **CDC On Boot** ativado, conforme a placa/bootloader.
- **Build reprodutível:** não instale Arduino globalmente; mantenha tudo em `toolchain/` para que todos usem a mesma versão.

---

## 7) (Histórico) Caminho antigo: ESP‑IDF puro

**Status:** descontinuado como caminho principal (muitos problemas de ambiente diferentes entre máquinas).

Setup exigia:

- Python 3.11 com pip funcional, venv/virtualenv.
- Geração do arquivo *constraints* (`%USERPROFILE%\.espressif\espidf.constraints.v5.5.txt`).
- Execução de `export.bat` corretamente no shell `cmd` (PowerShell variava *quoting*).

Mesmo com scripts (`bootstrap.ps1`, `build.cmd`), vimos erros recorrentes de:

- `idf_tools.py --non-interactive` indisponível vs versões diferentes.
- *constraints* faltando/bloqueadas pela rede.
- `pip` quebrado no Python embutido em algumas máquinas.

**Conclusão:** manteremos os scripts antigos só como referência, mas o método oficial é **Arduino CLI**.

---

## 8) Perguntas frequentes (FAQ)

- **Precisa instalar Arduino IDE no PC do cliente?**  
  Não. Só usamos o `arduino-cli.exe` dentro do repositório.

- **E a IDF?**  
  Vem “acoplada” pela *core* `esp32` do Arduino (a compilação ocorre em cima da IDF da core).

- **Consigo usar APIs IDF no meu código?**  
  Sim (`#include <esp_*>`), mas mantenha consistência — e, quando necessário, flags como `-DCONFIG_NIMBLE_CPP_IDF=1`.

- **E se um dia quisermos voltar ao IDF puro?**  
  Possível, mas o custo de setup por máquina foi alto. O CLI nos dá previsibilidade imediata.

---

## 9) Comandos de bolso

```powershell
# Atualizar índice + core
toolchain\arduino-cli.exe core update-index --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe core install esp32:esp32 --config-file toolchain\arduino-cli.yaml

# Ver boards e portas
toolchain\arduino-cli.exe board listall --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe board list    --config-file toolchain\arduino-cli.yaml

# Build / Upload / Monitor
toolchain\arduino-cli.exe compile --config-file toolchain\arduino-cli.yaml --fqbn esp32:esp32:esp32s3 --build-property compiler.cpp.extra_flags="-DCONFIG_NIMBLE_CPP_IDF=1" .
toolchain\arduino-cli.exe upload  --config-file toolchain\arduino-cli.yaml --fqbn esp32:esp32:esp32s3 -p COM4 .
toolchain\arduino-cli.exe monitor --config-file toolchain\arduino-cli.yaml -p COM4 -c baudrate=115200
```

---

## 10) Glossário rápido

- **IDF:** *Espressif IoT Development Framework* (SDK oficial do ESP32).
- **Arduino CLI:** ferramenta oficial em linha de comando para compilar/provisionar projetos Arduino (suporta *cores* como `esp32:esp32`).
- **FQBN:** *Fully Qualified Board Name*; identifica placa/target `vendor:core:board`.
- **NimBLE:** biblioteca BLE; aqui usamos IDF por baixo com headers integrados ao ambiente Arduino quando necessário.

---

## Anexo A — Por que este documento existe?

Este `instructions.md` foi pensado para “dar contexto” a qualquer IA/colaborador novo:

- Quem somos (Dyona, com apoio do Paulo).
- Qual o *stack* que de fato usamos agora (Arduino CLI sobre IDF).
- Como reproduzir exatamente a build numa máquina nova, sem instalar nada global.
- Armadilhas já vistas e como evitá‑las.

**Se você está lendo isso para começar agora:**

1. Garanta que `toolchain/arduino-cli.exe` e `toolchain/arduino-cli.yaml` existem (Seção 2.2).
2. Rode as Tasks do VS Code:
   - **Arduino (portable): Bootstrap CLI + Core (1x)**
   - **Arduino (portable): Build**
   - **Arduino (portable): Upload**
   - **Arduino (portable): Monitor**
3. Ajuste `sensegrid.serialPort` no `settings.json` e feliz compilação 🚀
