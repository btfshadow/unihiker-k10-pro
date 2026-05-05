# Spec 12 - Speech PTBR Artifacts and Packaging

## Objetivo
Consolidar o inventario tecnico da Fase 1 do plano PT-BR e definir um layout de pacotes para evoluir suporte real a `-DModel=PTBR` no `unihiker-pro`.

## Estado atual (contrato tecnico observado)

### 1) Contrato de selecao de modelo no build
No builder atual, o modelo e selecionado por `build_flags` com `-DModel=<tipo>`.

Implementacao atual aceita apenas:
- `CN`
- `EN`

Referencia:
- `external_packages/framework-arduinounihiker/tools/platformio-build.py`

### 2) Particoes de fala no firmware
A tabela de particoes define dois espacos relevantes para speech:
- `model` em `0x510000`, tamanho `4563k`
- `voice_data` em `0x985000`, tamanho `2542k`

Referencia:
- `external_packages/framework-arduinounihiker/tools/partitions/large_spiffs_16MB.csv`

### 3) Faixas de idioma da API ASR
A API atual expoe idioma apenas como:
- `lang=0` (CN)
- `lang=1` (EN)

Referencia:
- `external_packages/framework-arduinounihiker/libraries/asr/src/asr.h`

## Inventario de artefatos encontrados
Origem:
- `external_packages/framework-arduinounihiker/tools/partitions`

| Arquivo | Tamanho (bytes) | SHA-256 | Uso atual observado |
|---|---:|---|---|
| `srmodels.bin` | 3263658 | `38e9b67b1769ddbae421120a5f68692291c8bbc27f9f1c85a62eccdf40917150` | Bundle CN no builder atual |
| `srmodels4.bin` | 4370442 | `3c87330e5ad21ad8b9542fbb97388a382b6028ce7d3f2df35f14f268db676830` | Bundle EN no builder atual |
| `srmodels5.bin` | 4672178 | `2b265c85b0f4d45d12d54c46ef2042a1662905a4a4e97a0035f4796419620a73` | Presente no pacote, nao referenciado no builder atual |
| `esp_tts_voice_data_xiaoxin.dat` | 2602608 | `673881f05338b27cee7f071aa06121b3dacb0ecbe71dc80c31e57c092bbf30d3` | Voice data usado por CN/EN |

## Evidencias de linguagem nos modelos atuais
- `srmodels.bin`: referencia de modelo chines.
- `srmodels4.bin`: referencias a wake words EN (`Hi Telly`, `Jarvis`) e multinet ingles.
- `srmodels5.bin`: contem blocos chines e ingles; nao ha evidencias claras de PT-BR no pipeline atual.

Conclusao:
- Nao existe hoje caminho oficial PT-BR no builder nem na API de idioma.
- `srmodels5.bin` pode ser candidato tecnico para investigacao futura, mas nao pode ser assumido como PT-BR sem validacao funcional.

## Tabela de compatibilidade (Fase 1)
| Placa | Framework | Modelo build suportado hoje | Estado PT-BR |
|---|---|---|---|
| `unihiker_k10` | `framework-arduinounihiker` (linha atual) | `CN`, `EN`, `None` | Nao suportado nativamente |

## Proposta de layout de pacotes PT-BR (alvo)
Layout proposto no projeto (sem acoplamento de app legado):

- `unihiker-pro/assets/speech-models/ptbr/v1/srmodels_ptbr.bin`
- `unihiker-pro/assets/speech-models/ptbr/v1/tts_voice_ptbr.dat`
- `unihiker-pro/assets/speech-models/ptbr/v1/manifest.json`

Manifesto minimo (`manifest.json`) sugerido:
- `model_id`: identificador semantico (ex.: `ptbr-v1`)
- `asr_language`: `pt-BR`
- `wake_words`: lista de wake words
- `commands_profile`: perfil de comandos suportados
- `flash_offsets`: `{ model: "0x510000", voice_data: "0x985000" }`
- `checksums`: SHA-256 de cada artefato
- `min_framework_version`: versao minima suportada

## Criterio minimo de qualidade para pacote PT-BR
1. Integridade: checksum validado no build.
2. Compatibilidade: artefatos cabem nas particoes `model` e `voice_data`.
3. Seguranca de runtime: sem panic quando artefato ausente/corrompido (erro explicito + fallback).
4. Repetibilidade: deteccao de comandos PT-BR consistente em smoke controlado.

## Saidas executadas nesta Fase 1
- Contrato atual de build/runtime mapeado.
- Inventario de artefatos existentes com tamanho e hash.
- Gap PT-BR formalizado.
- Layout de pacote PT-BR proposto para implementacao nas proximas fases.

## Proximos passos (ponte para Fase 2)
1. Adicionar caminho `PTBR` no mecanismo de selecao de modelo do projeto.
2. Conectar `PTBR` ao layout de pacote proposto (manifest + checksums).
3. Implementar erro explicito/fallback quando pacote PT-BR nao existir.
