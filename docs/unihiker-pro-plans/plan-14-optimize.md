# Plan 14 - Optimize And PTBR Runtime Readiness

## Objetivo
foco é otimizar o código para usar abertura e fechamendo de funções de forma mais eficiente, e de otimizar o código para a placa que é um esp32-S3, ou seja tem 16 MB Flash, dualcore de 32bits com 240mhz, 512 KB de SRAM, 8 MB PSRAM, AES-128/256
Instruções de Vetor (SIMD): Suporte a operações Single Instruction, Multiple Data (SIMD) no processador Xtensa LX7, que acelera significativamente operações matriciais e de convolução usadas em redes neurais e processamento de sinais. 
Aceleração para Inteligência Artificial (AI): Instruções dedicadas para acelerar tarefas de machine learning embarcado (Edge AI), como detecção de faces e reconhecimento de voz, aproveitadas pelas bibliotecas ESP-DL, ESP-NN e ESP-DSP. 
Aceleração Criptográfica: Hardware dedicado para operações de segurança, incluindo:
AES-128/256
Hash (SHA)
RSA
Gerador de Números Aleatórios (RNG)
HMAC
Assinatura Digital
Coprocessador de Baixo Consumo (ULP): Inclui dois coprocessadores ULP — ULP-RISC-V e ULP-FSM — que permitem execução de tarefas mínimas em modos de baixa energia, como leitura de sensores durante o deep sleep.

## Escopo
- otimizar o código para a placa
- otimizar o código para que vire quase uma distribuição de programas.

## Fora de escopo (neste plano)
- Treinamento de modelo do zero no mesmo ciclo.
- Alteracoes invasivas no app legado em `external_packages` sem aprovacao explicita.

## Requisitos de projeto
- O `unihiker-pro` permanece como camada principal de evolucao (API simplificada + avancada).
- `external_packages` e referencia tecnica/base de compatibilidade.
- Otimizacao e testabilidade sao obrigatorias em cada fase.

## Decisao de arquitetura (2026-05-05)
- O pipeline de speech legado nao atende wake nativo em portugues (`ola luci`) com confiabilidade.
- A partir deste ponto, o fluxo principal passa a ser PTBR nativo, sem fallback automatico para EN no ambiente de smoke principal.
- Foi criado ambiente legado explicito apenas para comparacao e regressao:
	- `unihiker_k10_smoke`: estrito PTBR (`custom_ptbr_model_policy = error`)
	- `unihiker_k10_smoke_legacy`: fallback EN controlado (`custom_ptbr_model_policy = fallback_en`)

## Proximos passos imediatos
- Integrar artefatos PTBR nativos no manifesto `assets/speech-models/ptbr/v1/manifest.json`.
- Validar boot, wake `ola luci` e comandos `status` no ambiente estrito.
- Manter o ambiente legado apenas para benchmark comparativo, nao como baseline de produto.

## Provisionamento de firmware PTBR
- Script de provisionamento adicionado: `unihiker-pro/scripts/provision_ptbr_artifacts.py`.
- O script copia os artefatos PTBR para `unihiker-pro/assets/speech-models/ptbr/v1/`, atualiza checksums no `manifest.json` e recusa hashes legados CN/EN por padrao.
- Fluxo rapido:
	- `python unihiker-pro/scripts/provision_ptbr_artifacts.py --model <srmodels_ptbr.bin> --voice <tts_voice_ptbr.dat>`
	- `cd unihiker-pro/tests/speech_smoke && platformio run -e unihiker_k10_smoke`
	- upload manual para hardware apos build OK.

## TODO - Treino de gatilho PTBR e TTS
- [ ] Definir stack de treino para wake word PT-BR (`ola luci`) com pipeline reproduzivel.
- [ ] Montar dataset de wake: voces diferentes, ruidos reais, distancias variadas e frases negativas.
- [ ] Treinar e exportar artefato de wake/ASR para formato compativel com as particoes `model` e `voice_data`.
- [ ] Definir stack de TTS PT-BR (modelo + vocoder) com foco em latencia e tamanho para ESP32-S3.
- [ ] Gerar `srmodels_ptbr.bin` e `tts_voice_ptbr.dat` com checksums versionados no manifesto.
- [ ] Validar em hardware (taxa de acerto, falso positivo, tempo de resposta, estabilidade) e publicar relatorio.
