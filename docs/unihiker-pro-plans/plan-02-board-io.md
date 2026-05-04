# Plan 02 - Board IO

## Objetivo
Migrar inicialização de placa, pinos e entradas/saídas para camada HAL.

## Entregas
- Board HAL (`begin`, `digitalWrite`, `digitalRead`)
- Input service (botões)
- Output service (RGB e pinos)

## Fases
1. Isolar chamadas a `init_board`, `digital_write`, `digital_read`
2. Encapsular botões com debounce configurável
3. Padronizar mapeamento RGB e brilho

## Validação
- Exemplo de botão e LED equivalente ao legado

## Progresso desta implementação
- `IBoardHal` agora expõe `writePin`, `readPin` e callbacks de botão
- `LegacyBoardHal` encapsula `digital_write`, `digital_read` e adaptacao de `Button`
- `InputService`, `LedService` e novo `PinService` cobrem o slice inicial de Board IO
- `unihiker-pro` intercepta `init_board/digital_write/digital_read` via linker wrap para evitar o init binario quebrado do ES7243E em `0x15`
