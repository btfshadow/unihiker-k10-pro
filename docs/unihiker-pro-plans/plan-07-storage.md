# Plan 07 - Storage

## Objetivo
Estabelecer camada de armazenamento confiável para mídia e assets.

## Entregas
- Storage HAL
- Helpers de BMP/WAV
- API de verificação de cartão

## Fases
1. Mount/unmount e health check
2. Escrever foto BMP
3. Ler/escrever WAV
4. Formatos utilitários (JSON/CSV/TXT/BIN)
5. Leitura e manutenção de arquivos (exists/size/remove)

## Validação
- Operações de arquivo com tratamento de erro padronizado

## Progresso desta implementação
- `StorageService` agora expõe diretórios padrão por formato (`S:/images` para BMP e `S:/audio` para WAV)
- Adicionadas APIs de override por código: `setImageDirectory`, `setAudioDirectory`, `imagePath`, `audioPath`
- Adicionado `ensureDirectories()` para criar automaticamente as pastas padrão (e customizadas)
- Adicionado helper central `writeRgb565Bmp(...)` no `StorageService` para escrita BMP 16-bit (RGB565)
- Adicionado helper central de WAV com ciclo de gravação (`beginWavRecord`, `appendWavRecord`, `endWavRecord`) no `StorageService`
- Adicionado diretório padrão de dados (`S:/data`) com override por código (`setDataDirectory`, `dataPath`)
- Adicionados helpers utilitários de arquivo em `StorageService`: `writeTextFile`, `appendTextFile`, `writeBinaryFile`, `writeJson`, `writeCsv`
- Adicionado `healthCheck(...)` com probe opcional de leitura/escrita e métricas de capacidade (`totalBytes`/`usedBytes`)
- Adicionadas APIs de leitura e manutenção em `StorageService`: `readTextFile`, `readBinaryFile`, `appendBinaryFile`, `fileExists`, `fileSize`, `removeFile`
- `audio_smoke` atualizado para usar `StorageService` na resolução de caminho WAV, permitindo customização por código
- `audio_smoke` migrado para gravar WAV via `StorageService`, removendo lógica duplicada de header/chunk/finalização
- `storage_smoke` consolidado para validação all-in-one: health check + diretórios + BMP/WAV + JSON/CSV/TXT/BIN + leitura + append + exists/size/remove

## Status de fase
- Fase 1 concluída: mount/check com `initSd` + `healthCheck`
- Fase 2 concluída: helper BMP central no `StorageService`
- Fase 3 concluída: helper WAV central e migração do fluxo de gravação do `audio_smoke`
- Fase 4 concluída: formatos utilitários (JSON/CSV/TXT/BIN)
- Fase 5 concluída: leitura e manutenção de arquivos (exists/size/remove)
