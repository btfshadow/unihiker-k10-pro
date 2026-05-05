# Plan 10 - Connectivity

## Objetivo
Adicionar conectividade como módulo opcional, independente do core.

## Status
- Em andamento.
- Escopo ampliado para conectividade completa em modo SDK (nao apenas wrapper minimo).
- Fase 1 concluida (Wi-Fi SDK completo) e Fase 2 iniciada (mDNS + diagnostico local).
- Fase 3 iniciada com servidor HTTP opcional para diagnostico remoto.

## Entregas
- Connectivity service completo em `unihiker-pro` com API para projetos importadores:
	- perfis conhecidos persistidos (NVS)
	- fast reconnect com dicas de radio (canal + BSSID)
	- fallback inteligente (tenta conhecidos antes de scan global)
	- scan de redes com metadados
	- parser/config por QRCode no formato `WIFI:T:...;S:...;P:...;;`
	- ferramentas de analise de ambiente Wi-Fi (densidade de canal, top RSSI, qualidade)
- Hooks para mDNS e HTTP (fase seguinte)
- Feature flags por build

## Progresso atual
- `ConnectivityService` agora inclui controle de mDNS:
	- `startMdns(...)`, `stopMdns()`, `mdnsLinkStats(...)`, `mdnsDiagnostics(...)`
- `connectivity_smoke` foi expandido com comandos seriais:
	- `qr listen` (abre AI QR, espera payload Wi-Fi compativel e tenta conexao)
	- `mdns start <host>`
	- `mdns stop`
	- `mdns stats`
	- `mdns query`
	- `http start [port]`, `http stop`, `http stats`

- `ConnectivityService` agora inclui servidor HTTP opcional com endpoints:
	- `GET /health`
	- `GET /wifi/stats`
	- `GET /mdns/stats`
	- `GET /wifi/analyze`

## Fases
1. Base de Wi-Fi SDK completa (perfis, fast reconnect, scan fallback, QR, analise)
2. mDNS e diagnóstico de descoberta local
3. Endpoints HTTP opcionais e utilitarios web

## Validação
- Conexao estavel e status reportavel
- Reconexao rapida apos reboot sem scan completo quando perfil conhecido esta disponivel
- Fluxo de configuracao por QRCode validado em smoke
- Relatorio de analise de redes disponivel via API/serial
