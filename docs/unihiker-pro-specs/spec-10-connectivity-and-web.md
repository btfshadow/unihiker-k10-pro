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
- API mínima: connect/disconnect/status/ip
- Extensões opcionais para HTTP e descoberta local
- Design para feature flags (habilitar/desabilitar por build)
