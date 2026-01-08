# Calibracao SenseGrid (A4) - passo a passo

Objetivo: coletar baseline de ambiente vazio (~60-120 s), sugerir parametros (range, snr_min, holds) e aplicar sem depender de endpoint especifico. Tudo em ASCII (sem acento).

## Requisitos
- Firmware atual (HTTP/MQTT/CLI funcionando).
- Broker Mosquitto rodando (ex.: `mosquitto -c mosquitto.config -v`) acessivel na rede local.
- PC na mesma rede que a ESP32 ou conectado ao AP da placa.


## Provisionamento de rede
- CLI: `wifi show`, `wifi set <ssid> <pass>`, `wifi ap <ssid> [pass]`, `wifi clear [sta|ap|all]`, `wifi apply`.
- HTTP: `GET /v1/net`, `POST /v1/net` (JSON com `sta_ssid`, `sta_pass`, `ap_ssid`, `ap_pass`, `clear`).
- Portal AP: se nao houver STA, o firmware sobe AP aberto `SenseGrid-Setup-XXXX` e a pagina `http://192.168.4.1/setup`.

## Fluxo via CLI serial (USB)
1) Abra o serial (115200). No VS Code: Terminal -> New Terminal -> `toolchain\arduino-cli.exe monitor --config-file toolchain\arduino-cli.yaml -p COM5 -c baudrate=115200`.
2) Inicie coleta (vazio, saia do alcance):  
   `calib start 60000`  (ou 120000).
3) Consulte progresso:  
   `calib status`
4) Veja sugestao antes de aplicar:  
   `calib preview`
5) Aplique em memoria e NVS:  
   `calib apply`
6) Opcional reset/abort:  
   `calib reset` ou `calib abort`
7) Conferir pipeline:  
   `pipe show`

## Fluxo via MQTT (controle remoto)
Use arquivos JSON para evitar problemas de aspas no PowerShell.

1) Comandos (salve cada linha em arquivos):
```
echo {"op":"calib.start","dur_ms":60000,"txid":"t1"} > cmd_start.json
echo {"op":"set","path":"pipe.dist_max","value":400,"txid":"t2"} > cmd_set.json
```
2) Publicar:
```
mosquitto_pub -h <broker_host> -t "spsb01/<device_id>/c" -f cmd_start.json
mosquitto_pub -h <broker_host> -t "spsb01/<device_id>/c" -f cmd_set.json
```
3) Ouvir respostas (acks/erros):
```
mosquitto_sub -h <broker_host> -t "spsb01/<device_id>/ack" -t "spsb01/<device_id>/err" -v
```
4) Para acompanhar medidas:
```
mosquitto_sub -h <broker_host> -t "spsb01/<device_id>/meas_raw" -C 5 -v
```
Hint:
- Se tiver nome configurado (DNS/hosts), use hostname estavel (ex.: broker.sensegrid) para nao depender de IP variavel.
- Se nao tiver DNS/hosts, use o IP do broker (ex.: `-h 192.168.15.11`). Para hostname local no Windows, pode adicionar em `C:\Windows\System32\drivers\etc\hosts`: `192.168.15.11 broker.sensegrid`.

## Fluxo via HTTP (consulta)
- Status de rede: `curl http://<ip>/v1/net`
- Portal: `http://<ip>/setup` (HTML)
- Ocupacao: `curl http://<ip>/v1/occupancy`
- Medida completa: `curl http://<ip>/v1/meas`
- Health: `curl http://<ip>/v1/health`
- Nota: POST /v1/cmd hoje apenas responde ack generico (nao executa calib/set).

## Requisitos de broker
- Listener 1883 aberto para a rede onde a ESP esta.
- `allow_anonymous true` ou usuario/senha configurados conforme politica local.
- Para testes rapidos: `mosquitto -c mosquitto.config -v` (arquivo ja no repo).
