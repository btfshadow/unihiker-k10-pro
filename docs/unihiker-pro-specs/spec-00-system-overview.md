# Spec 00 - System Overview

## Escopo
O SDK atual do UNIHIKER K10 é centrado na biblioteca `unihiker_k10` com acoplamento direto entre UI, câmera, áudio, IA, sensores, cartão SD e controle de pinos via expansor I2C.

## Componentes principais
- Board: ESP32-S3 (16 MB flash, PSRAM habilitada)
- Tela: ILI9341, 240x320
- Câmera: GC2145 (captura RGB565/QVGA)
- Áudio: I2S full-duplex (mic + amp)
- Sensores: AHT20, LTR303ALS, SC7A20H
- IO expandido: XL9535 (botões, backlight, reset de câmera, gain)
- Armazenamento: microSD FAT
- IA: face, cat, movimento, QR
- Voz: ASR + TTS

## Problemas estruturais observados
- Acoplamento forte (UI + drivers + tarefas FreeRTOS no mesmo módulo)
- Estado global compartilhado (filas, mutexes, objetos LVGL)
- APIs com responsabilidades misturadas
- Dificuldade de teste unitário e evolução incremental

## Diretriz para unihiker-pro
Arquitetura em camadas:
1. Core (tipos comuns, config, estados, contratos)
2. HAL (drivers e adaptação de hardware)
3. Services (regras e casos de uso)
4. Facade/API pública estável
5. Apps/Examples separados

## Metas de engenharia
- Evolução sem quebrar funcionalidades essenciais
- Testabilidade por módulo
- Menor acoplamento entre features
- Reuso de drivers originais quando possível
