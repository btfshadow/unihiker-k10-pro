# Plan 13 - Speech PT-BR Model Pipeline

## Objetivo
Viabilizar suporte real a PT-BR no fluxo de fala do `unihiker-pro` (ASR/TTS), mantendo a implementacao orientada a produto proprio e sem depender de mudancas de fluxo de app em `external_packages`.

## Escopo
- Definir artefatos de modelo PT-BR necessarios (wake word, comandos, voz TTS).
- Adicionar suporte de builder para selecionar PT-BR no build/upload.
- Centralizar a orquestracao no `unihiker-pro` (config, validacao, fallback e diagnostico).

## Fora de escopo (neste plano)
- Treinamento de modelo do zero no mesmo ciclo.
- Alteracoes invasivas no app legado em `external_packages` sem aprovacao explicita.

## Requisitos de projeto
- O `unihiker-pro` permanece como camada principal de evolucao (API simplificada + avancada).
- `external_packages` e referencia tecnica/base de compatibilidade.
- Otimizacao e testabilidade sao obrigatorias em cada fase.

## Entregas
1. Especificacao de artefatos PT-BR
- Mapa de arquivos obrigatorios (ASR/TTS), offsets de flash e versoes.
- Politica de naming e checksum para validacao de integridade.

2. Suporte de builder para PT-BR
- Suporte a `-DModel=PTBR` em pipeline de build/upload do projeto.
- Rota de fallback segura quando artefato PT-BR nao existir (erro explicito, sem panic em runtime).

3. Integracao no `unihiker-pro`
- Config de speech no service/facade com selecao de idioma/modelo.
- Logs de diagnostico claros: modelo selecionado, artefatos carregados, status de wake words.

4. Testes
- Smokes de speech cobrindo inicializacao, reconhecimento de comandos e TTS para PT-BR.
- Testes unitarios (quando aplicavel) para parser/configuracao de modelo e regras de fallback.

## Fases
### Fase 1 - Catalogo de artefatos PT-BR
- Levantar formato esperado pelo runtime atual (arquivos, offsets e tamanho).
- Definir pacote PT-BR alvo e criterio minimo de qualidade.
- Criar tabela de compatibilidade por placa/firmware.

Status: executada.
Saida tecnica: `docs/unihiker-pro-specs/spec-12-speech-ptbr-artifacts.md`

### Fase 2 - Builder PT-BR
- Implementar caminho `PTBR` no mecanismo de selecao de modelos do projeto.
- Validar mensagens de erro para caso de artefato ausente/corrompido.
- Garantir que EN/CN continuem funcionais (nao regressao).

Status: executada.
Evidencia de validacao (speech_smoke):
- Build `platformio run -e unihiker_k10_smoke` em `unihiker-pro/tests/speech_smoke` concluido com sucesso.
- Log confirmou fallback seguro quando artefatos PT-BR nao existem:
	- `[PTBR] WARNING: PTBR artifact(s) not found ...`
	- `[PTBR] Fallback -> EN model bundle`

### Fase 3 - Runtime no unihiker-pro
- Expor selecao de perfil de fala no service/facade.
- Ajustar fluxo de init para evitar crash quando wake words nao estiverem disponiveis.
- Adicionar telemetria de inicializacao e estado.

Status: em andamento.
Progresso atual:
- `SpeechService` agora expoe selecao de perfil em runtime (`AUTO`, `CN`, `EN`, `PTBR`) via facade.
- Inicializacao com fallback controlado para EN foi adicionada (`beginWithProfile(...)`, `beginAuto(...)`).
- Telemetria de init foi adicionada (`initSummary()` + log `speech.init ...`) incluindo perfil solicitado/efetivo, idioma, wake e fallback.
- Build validado no smoke de speech apos integracao das mudancas.
- Workaround aplicado no service layer para bug do framework: `ASR::addASRCommand(uint8_t, String)` entra em recursao e estoura stack; cadastro de comandos agora e roteado para overload `char*`.
- Mitigacao adicional de runtime: `SpeechService::speak(...)` agora protege chamada de TTS quando `xQueueTTS` nao esta inicializada, evitando assert em `xQueueGenericSend` com backend parcial.
- `speech_smoke` atualizado para registrar comandos ASR apenas apos janela de aquecimento do backend (registro tardio), reduzindo erros `MN_COMMAND ... not initialize` durante boot.
- `speech_smoke` agora explicita wake phrase de runtime atual (`hi telly`/`jarvis`) e adiciona aliases PT (ex.: `ola luci`/`ola lusi`, `status luci`/`estado luci`) para melhorar reconhecimento no modelo EN.
- Estado de runtime exposto no service para orquestracao segura: `wakeDetected()` e `ttsReady()`.
- Ajustes de confiabilidade no smoke: janela de wake ampliada para 12s, aliases EN adicionais para comando, e menor verbosidade (`CORE_DEBUG_LEVEL=1`) para reduzir contencao no pipeline de audio.
- Interface de cadastro de comandos adicionada ao `SpeechService`: `resetCommandRegistry()`, `queueCommand(...)`, `applyQueuedCommands()` e contador de fila.
- `speech_smoke` passou a usar essa interface (perfil de comandos PT/EN em fila) e tambem aceita cadastro em runtime por serial (`add1 <frase>`, `add2 <frase>`, `stats`).
- Inicializacao de TTS agora tem API explicita (`initTts(speed)`), com tentativa antecipada no boot e fallback seguro sem assert.
- Cadastro de comandos agora pode ser persistido no smoke (Preferences/NVS) e recarregado no boot (`save`, `clearpersist`).
- Calibracao guiada adicionada no smoke com alvo por frase + hits por comando (`calib on/off/next/prev/reset`) e navegacao por botoes (`A` anterior, `B` proximo).
- Modo de wake virtual PT adicionado no smoke: deteccao de frases de saudacao (ex.: `ola luci`) abre uma sessao temporaria de comandos, reduzindo dependencia pratica do wake EN fixo (`hi telly/jarvis`) no fallback.

Limitacoes confirmadas no estado atual:
- Com fallback EN ativo, wake words continuam do modelo EN (nao PT-BR).
- TTS pode permanecer indisponivel (`xQueueTTS` nula) dependendo da inicializacao do backend legado; fluxo agora evita assert e segue com ASR.

Pendente desta fase:
- Validacao em hardware do comportamento de fallback na inicializacao quando perfil PTBR e solicitado sem artefato PTBR.
- Consolidar uso da nova API de perfil em mais smokes/casos de exemplo.

### Fase 4 - Validacao e release
- Rodar matriz de smokes (EN, CN, PTBR) com comparativo de estabilidade.
- Definir checklist de release para publicacao no GitHub/PlatformIO.

## Criterios de aceite
- Build com `-DModel=PTBR` injeta artefatos corretos ou falha com erro explicito e acionavel.
- Runtime nao entra em panic por falta de wake words (fallback controlado).
- Comandos PT-BR de smoke sao reconhecidos de forma repetivel em ambiente controlado.
- API simplificada e avancada continuam coerentes e documentadas.

## Riscos e mitigacoes
- Ausencia de modelo PT-BR compativel: liberar com fallback e instrucoes de provisao de artefato.
- Regressao em EN/CN: matriz de smoke obrigatoria antes de merge.
- Acoplamento ao legado: manter adaptadores e configuracoes no `unihiker-pro`.

## Proximo passo imediato
- Concluir Fase 3 com validacao em hardware do init/fallback/telemetria e ajuste final da API de perfil de fala no facade.
