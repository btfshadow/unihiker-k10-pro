# Spec 01 - Board And Pins

## Base da placa
- Nome: UNIHIKER K10
- Vendor: DFRobot
- MCU: esp32s3
- Framework: arduino + espidf
- Flags relevantes:
  - `-DARDUINO_USB_MODE=1`
  - `-DBOARD_HAS_PSRAM`

## I2C interno
- `SDA=GPIO47`
- `SCL=GPIO48`
- Barramento compartilhado para sensores e expansor XL9535

## Endereços I2C validados
- `0x11`: codec ES7243E (observado em scan real)
- `0x19`: SC7A20H (acelerômetro)
- `0x20`: XL9535 (expansor IO)
- `0x29`: LTR303ALS (luz)
- `0x38`: AHT20 (temp/umidade)

## Expansor de IO (XL9535)
Enum `ePin_t` no `initBoard.h`:
- Port0: `eLCD_BLK`, `eCamera_rst`, `eP11_KeyB`, `eP12`, `eP13`, `eP14`, `eP15`, `eP2`
- Port1: `eP8`, `eP9`, `eP10`, `eP6`, `eP5_KeyA`, `eP4`, `eP3`, `eAmp_Gain`

## USB serial
- `Serial` pode mapear UART0 (pinos de câmera em algumas configurações)
- `USBSerial` deve ser preferido para debug via USB CDC

## Requisitos para unihiker-pro
- Definir mapa de pinos centralizado em um módulo HAL
- Evitar números mágicos dispersos
- Expor API para remapeamento e diagnóstico de barramento
