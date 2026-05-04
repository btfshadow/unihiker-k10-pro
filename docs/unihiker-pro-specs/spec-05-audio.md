# Spec 05 - Audio

## Arquitetura atual
- Barramento I2S único para captura e reprodução
- Configuração em `UNIHIKER_K10::initI2S()`
- Playback de tons e melodias embutidas via classe `Music`
- Playback/recording em WAV com SD (`playTFCardAudio`, `recordSaveToTFCard`)

## API atual observada
- `playMusic(melody, options)`
- `playTone(freq, beat)`
- `stopPlayTone()`
- `playTFCardAudio(path)`
- `stopPlayAudio()`
- `recordSaveToTFCard(path, seconds)`

## Características
- Mutua exclusão por mutex I2S
- Ajuste dinâmico de sample rate
- WAV header montado manualmente

## Riscos
- Conflito entre tarefas de áudio e leitura de MIC
- Controle de estado global (`_stopMusic`, `_stopState`, `playMusicState`)
- Sem mixer/arbiter para múltiplas fontes

## Requisitos para unihiker-pro
- Audio HAL com sessão única e arbitragem de uso
- Services separados: tone, melody, file player, recorder
- Estado interno encapsulado por instância
- Contratos explícitos para formato de áudio
