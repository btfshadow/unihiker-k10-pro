# Spec 10 - Connectivity And Web

## Recursos disponíveis no framework
- Wi-Fi
- Bluetooth/BLE
- mDNS
- Web server/HTTP utilities

## Evidências no SDK atual
- Includes presentes em `unihiker_k10.h/.cpp`:
  - `app_wifi.h`
  - `app_httpd.hpp`
  - `app_mdns.h`

## Situação atual
- Funcionalidades de rede não expostas de forma consistente na API de alto nível
- Forte dependência de módulos internos do framework

## Requisitos para unihiker-pro
- Connectivity service separado do core multimídia
- API de Wi-Fi para uso SDK/importador:
  - connect/disconnect/status/ip
  - perfis conhecidos persistidos em NVS
  - reconexao rapida no reboot sem scan total quando possivel
  - scan completo com metadados (RSSI/canal/BSSID/seguranca)
  - parse/config de credenciais por QRCode (`WIFI:T:...;S:...;P:...;;`)
  - fluxo de escuta ativa via AI QR (abrir modo QR, aguardar codigo compativel e conectar)
  - analise de ambiente Wi-Fi para diagnostico em campo
- API de descoberta local (mDNS) para uso SDK/importador:
  - start/stop de anunciante (`host`, `instance`, `service`, `proto`, `port`)
  - stats de runtime de mDNS
  - diagnostico com query de host/servico na rede local
- API HTTP opcional para diagnostico remoto:
  - start/stop de servidor HTTP embarcado
  - endpoint `GET /health`
  - endpoint `GET /wifi/stats`
  - endpoint `GET /mdns/stats`
  - endpoint `GET /wifi/analyze`
- Extensões opcionais para HTTP e descoberta local
- Design para feature flags (habilitar/desabilitar por build)
