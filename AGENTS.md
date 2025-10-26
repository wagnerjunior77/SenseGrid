# SenseGrid — AGENTS.md (for CODEX/automation)

> **What this file is:** Operating instructions for code agents (e.g., OpenAI CODEX or similar automations) working in this repository.  
> **Target audience:** non-human assistants + new contributors.  
> **Status (now):** **Atividade 2 — Bring‑up de hardware & drivers**.

---

## 0) Project overview (quick brief)

**SenseGrid (Dyona)** is a presence/motion sensing firmware for **ESP32‑S3** that fuses a radar module with environmental sensors (temp/humidity/lux), exposes data via local API/MQTT, and targets low latency (< 1 s) with < 5% FP/FN.  
We standardized builds on **Arduino CLI (portable inside the repo)** to avoid machine‑specific ESP‑IDF/Python setup issues. Arduino‑ESP32 runs **on top of** Espressif’s IDF, so we can mix Arduino libraries with IDF headers/APIs when needed.

---

## 1) Current phase — Atividade 2 (Bring‑up de hardware & drivers)

**Goal:** electrical + software bring‑up of the radar, serial configuration, and initial parsing (distance, speed, SNR). Add drivers for environmental sensors (temp/umid/lux) and a simple **CLI de diagnóstico** (serial) to stream real‑time readings.  
**Deliverables:** *hello‑radar* firmware running + CLI with stable/consistent readings.

### Acceptance checklist (agent MUST aim for this)
- [ ] Compiles reproducibly on a clean Windows machine using repo‑local toolchain (no global installs).  
- [ ] Serial port configured and opened; simple loop prints “radar alive” at 1 Hz even without radar attached.  
- [ ] If radar is present, parse and print fields `{distance, speed, rssi/SNR}` (best‑effort).  
- [ ] Stub drivers for env sensors compiled (graceful fallback when hardware not present).  
- [ ] CLI commands (over Serial @115200): `help`, `version`, `radar?`, `env?` (return JSON lines).  
- [ ] Build artifacts exported to `toolchain/build/SenseGrid/` via VS Code task **Build (export)**.

> **Note:** We are still *pre‑pipeline*. Do not implement state machine/classifier yet (that’s Atividade 3).

---

## 2) Repository layout (expected)

```
<sensegrid-root>/
  sketch/
    SenseGrid/
      SenseGrid.ino                  # minimal Arduino entry
      extern_core.c                  # wrapper (calls sg_core_init)
      extern_adapters_logger.c       # wrapper (calls sg_adapters_logger_init)
      sg_core.cpp / sg_core.h        # lightweight stubs for now
      sg_adapters_logger.cpp / .h    # lightweight stubs for now
  components/
    common/include/                  # shared headers
    core/include/                    # (headers only for now; keep impl in sketch for build sanity)
    drivers_env/include/
    drivers_radar/include/
    parser_radar/include/
    adapters_http/include/
    adapters_mqtt/include/
    adapters_logger/include/
    adapters_flashrepo/include/
    ports/include/
  toolchain/
    arduino-cli.exe                  # portable CLI (committed by bootstrap or user)
    arduino-cli.yaml                 # CLI config (in repo)
    build-arduino.ps1                # uses include-dirs.txt -> build.extra_flags
    include-dirs.txt                 # one include dir per line (relative paths are OK)
    build/                           # compiled binaries (generated)
    arduino-data/                    # Arduino caches (generated, gitignored)
    arduino-downloads/               # downloads cache (generated, gitignored)
  .vscode/
    tasks.json                       # VS Code tasks (see §3)
    settings.json                    # shared settings (see §3)
  docs/
    cronograma_sensegrid.md          # schedule (Atividade 2 in progress)
  AGENTS.md                          # THIS file
  .gitignore
```

> Keep **headers** in `components/*/include`. For now, **implementations that must link** live in `sketch/SenseGrid` to avoid missing symbols while drivers mature.

---

## 3) Build & tasks (portable Arduino CLI)

We rely on **repo‑local** CLI (`toolchain/arduino-cli.exe`) and config (`toolchain/arduino-cli.yaml`).

### 3.1 Required VS Code settings
`.vscode/settings.json` must define:
```jsonc
{
  "sensegrid.fqbn": "esp32:esp32:esp32s3",
  "sensegrid.serialPort": "COM4",
  "sensegrid.coreVersion": "3.0.7" // keep this pinned unless explicitly asked to upgrade
}
```
> **Do not auto‑upgrade** the Arduino‑ESP32 core; we pin **3.0.7** due to prior network/toolchain issues. Update only on request.

### 3.2 Include paths control
`toolchain/include-dirs.txt` — one path per line (comments with `#` allowed). Example:
```
# Base headers
./components/common/include
./components/core/include
./components/drivers_radar/include
./components/drivers_env/include
./components/parser_radar/include
./components/adapters_logger/include
```
The build task consumes this file and injects `-I...` via **build.extra_flags**.

### 3.3 Tasks: sequence to run on a fresh machine
1) **Arduino (portable): Bootstrap CLI (1x)** — ensures CLI present (if provided).  
2) **Arduino (portable): Install Core (1x ou quando trocar versão)** — installs `esp32:esp32@${config:sensegrid.coreVersion}` using our YAML.  
3) **Arduino (portable): Build (export)** — compiles `sketch/SenseGrid` and exports binaries to `toolchain/build/SenseGrid/`.  
4) *(Later)* **Upload** and **Monitor** when hardware is connected.

> If tasks are missing, agent should create/update `.vscode/tasks.json` to match the above. Keep commands Windows‑friendly (escape backslashes with `\\\\`).

### 3.4 CLI (manual)
```powershell
# Export binaries to toolchain/build/SenseGrid
toolchain\arduino-cli.exe compile ^
  --config-file toolchain\arduino-cli.yaml ^
  --fqbn esp32:esp32:esp32s3 ^
  --export-binaries ^
  --build-path toolchain\build\SenseGrid ^
  .\sketch\SenseGrid
```

---

## 4) Coding guidelines for this phase

- **Serial @115200** default. Provide readable logs and a **minimal CLI** via simple commands (line‑based).  
- **No hardware hard‑fail:** if a sensor/radar isn’t present, print a warning and continue (return stub JSON).  
- **Headers vs sources:** new headers go under `components/*/include`, but **implement** in `sketch/SenseGrid` unless the link is guaranteed.  
- **Namespaces/files:** keep names `sg_*` to avoid clashes with Arduino/IDF.  
- **Testing without hardware:** keep a `SANITY` mode (e.g., `#ifdef SG_SANITY`) that prints mock frames each 200 ms for parser validation.

Example JSON lines for CLI:
```json
{"t":"hello","fw":"0.1.0","board":"esp32s3"}
{"t":"radar","distance_m":1.82,"speed_mps":0.03,"snr_db":17.2}
{"t":"env","temp_c":24.5,"rh":53.1,"lux":120.0}
```

---

## 5) What CODEX should change or NOT change

**OK to change (scope Atividade 2):**
- Create/update **drivers_radar** (UART pins/baud configurable), simple frame parser, and mocks.  
- Add **drivers_env** placeholders that compile and provide stub values when hardware missing.  
- Implement **CLI de diagnóstico** (commands + JSONL printing).  
- Update `.vscode/tasks.json` and `toolchain/include-dirs.txt` as needed.  
- Update **docs** under `docs/` with short HOWTOs.

**Do NOT change (without explicit instruction):**
- Do not bump Arduino core above **3.0.7**.  
- Do not introduce global Arduino/IDF installs; use `toolchain/`.  
- Do not move `components/*/include` out of place.  
- Do not delete cache folders from history (they’re already gitignored).  
- Do not add heavy dependencies that fetch gigabytes.

---

## 6) Troubleshooting (known issues)

- **Linker: “undefined reference to `main` / `delay` / `Serial0`”**  
  Happens when mixing C/C++ or moving implementations out of the sketch too soon. Keep wrappers (`extern_*.c`) small and ensure stubs (`sg_core_init`, `sg_adapters_logger_init`) exist in the sketch during bring‑up.

- **Network timeouts when installing core**  
  We pin **3.0.7**. If an upgrade is required, open a PR that first updates `.vscode/settings.json` (`sensegrid.coreVersion`) then re‑runs *Install Core* task.

- **Missing includes**  
  Always add headers to `toolchain/include-dirs.txt` and let the build task push `-I...` via `build.extra_flags`.

---

## 7) Deliverables to produce in this phase (by the agent)

- [ ] Source implementing **CLI de diagnóstico** (commands: `help`, `version`, `radar?`, `env?`).  
- [ ] Radar UART config + basic parser with mock fallback.  
- [ ] Env drivers stubs (return fixed values if sensor absent).  
- [ ] Update docs: `docs/bringup-notes.md` (wiring pins, UART config, known boards).  
- [ ] Ensure **Build (export)** task creates artifacts in `toolchain/build/SenseGrid/`.

---

## 8) PR checklist (for CODEX)

- [ ] `sketch/SenseGrid` compiles on CI/local with **no hardware attached**.  
- [ ] `toolchain/include-dirs.txt` updated if new headers added.  
- [ ] `.vscode/tasks.json` and `.vscode/settings.json` remain valid; no absolute user paths.  
- [ ] No global install steps in README; all commands use `toolchain/arduino-cli.exe`.  
- [ ] Binaries are **not** committed; they’re generated into `toolchain/build/` and gitignored.  
- [ ] Docs updated (1–2 pages max) and cross‑linked from `docs/` index.

---

## 9) Quick commands (copy/paste)

```powershell
# 1) Install core (once or when version changes)
toolchain\arduino-cli.exe core update-index --config-file toolchain\arduino-cli.yaml
toolchain\arduino-cli.exe core install esp32:esp32@3.0.7 --config-file toolchain\arduino-cli.yaml

# 2) Build (export artifacts)
toolchain\arduino-cli.exe compile ^
  --config-file toolchain\arduino-cli.yaml ^
  --fqbn esp32:esp32:esp32s3 ^
  --export-binaries ^
  --build-path toolchain\build\SenseGrid ^
  .\sketch\SenseGrid

# 3) Monitor (when device present)
toolchain\arduino-cli.exe monitor ^
  --config-file toolchain\arduino-cli.yaml ^
  -p COM4 ^
  -c baudrate=115200
```

---

### Final note
If you’re an automated agent: prefer **incremental** PRs. Keep changes small, buildable, and scoped to Atividade 2’s objectives. Human reviewers will provide pin assignments and radar specifics when hardware arrives.
