# Plan 10 - Connectivity

## Objetivo
Adicionar conectividade como módulo opcional, independente do core.

## Status
- Em andamento.
- Escopo ampliado para conectividade completa em modo SDK (nao apenas wrapper minimo).

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

## Fases
1. Base de Wi-Fi SDK completa (perfis, fast reconnect, scan fallback, QR, analise)
2. mDNS e diagnóstico de descoberta local
3. Endpoints HTTP opcionais e utilitarios web

## Validação
- Conexao estavel e status reportavel
- Reconexao rapida apos reboot sem scan completo quando perfil conhecido esta disponivel
- Fluxo de configuracao por QRCode validado em smoke
- Relatorio de analise de redes disponivel via API/serial
