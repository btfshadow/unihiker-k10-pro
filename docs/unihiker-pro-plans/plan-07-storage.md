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

## Validação
- Operações de arquivo com tratamento de erro padronizado

## Progresso desta implementação
- `StorageService` agora expõe diretórios padrão por formato (`S:/images` para BMP e `S:/audio` para WAV)
- Adicionadas APIs de override por código: `setImageDirectory`, `setAudioDirectory`, `imagePath`, `audioPath`
- Adicionado `ensureDirectories()` para criar automaticamente as pastas padrão (e customizadas)
- `audio_smoke` atualizado para usar `StorageService` na resolução de caminho WAV, permitindo customização por código

## Status de fase
- Fase 1 iniciada com mount/check e criação de diretórios padronizados por formato
- Próximo incremento: mover helpers BMP/WAV para uso central em `StorageService`
