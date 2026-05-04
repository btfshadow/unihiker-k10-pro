# Spec 06 - Storage And Filesystem

## Implementação atual
- `SD.begin()` para inicializar cartão
- Integração LVGL FS (`lv_fs_fatfs_init`)
- Fotos salvas em BMP 16-bit
- Áudio em WAV (PCM)

## Caminhos usados
- Convenção observada nos exemplos: `S:/arquivo.ext`

## Operações principais
- Inicialização do SD
- Escrita de arquivo de imagem BMP
- Leitura/escrita de áudio WAV
- Carregamento de imagem para canvas

## Problemas atuais
- Loops potencialmente infinitos em falha de SD
- Código de IO espalhado entre módulos
- Falta de camada de validação de path/formato

## Requisitos para unihiker-pro
- Storage HAL com mount/unmount/check
- API de arquivo segura com retornos de erro
- Helpers para formatos multimídia (BMP/WAV)
- Estratégia de recuperação para falhas de cartão
