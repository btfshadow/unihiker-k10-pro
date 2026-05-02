# 15 — Fontes e QR-Code

## 1. Font chip GT30L24A3W

Ver `13-lib-sensors.md §7` para API. Esta seção detalha o uso pela camada LVGL.

### 1.1 Adapter LVGL (em `unihiker_k10.cpp`)

Cria duas fontes "virtuais" LVGL que despacham bitmaps a quente:

```cpp
const lv_font_t my_custom_font_16 = {
  .get_glyph_dsc    = myGetGlyphDscCb_16,
  .get_glyph_bitmap = myGetGlyphBitmapCb_16,
  .line_height = 15,
  .base_line   = 3,
  ...
};
const lv_font_t my_custom_font_24 = {
  .get_glyph_dsc    = myGetGlyphDscCb_24,
  .get_glyph_bitmap = myGetGlyphBitmapCb_24,
  .line_height = 27,
  .base_line   = 3,
  ...
};
```

Callbacks:

```cpp
bool myGetGlyphDscCb_24(font, dsc_out, unicode_letter, unicode_next) {
  dsc_out->box_h = 24;
  dsc_out->box_w = 24;
  dsc_out->adv_w = (unicode < 128) ? ASCII_GetInterval(unicode, ASCII_24_B) : 24;
  dsc_out->bpp   = 1;          // monocromático
  dsc_out->is_placeholder = false;
  return true;
}

const uint8_t* myGetGlyphBitmapCb_24(font, unicode_letter) {
  if (unicode_letter < 128) {
    ASCII_GetData(unicode_letter, ASCII_24_B, _pBits);
  } else {
    uint16_t u2g = U2G(unicode_letter);
    GBK_24_GetData((u2g >> 8) & 0xff, u2g & 0xff, _pBits);
  }
  return _pBits;
}
```

> Limitação: `_pBits` é estático global ⇒ não thread-safe. Acesso ao font chip
> é serializado pelo `xLvglMutex` (LVGL chama os callbacks dentro do
> `lv_task_handler`).

### 1.2 Conversão Unicode → GBK (`U2G`)

Funciona apenas para o *Basic Multilingual Plane* (CJK Unified Ideographs).
Internamente faz lookup em uma tabela embutida no font chip ou em PSRAM.

## 2. `lv_qrcode` (lib `lv_lib_qrcode`)

Pasta: [external_packages/framework-arduinounihiker/libraries/lv_lib_qrcode/](../external_packages/framework-arduinounihiker/libraries/lv_lib_qrcode/)

API LVGL minimalista:

```c
lv_obj_t *lv_qrcode_create(lv_obj_t *parent, lv_coord_t size,
                           lv_color_t dark_color, lv_color_t light_color);
lv_res_t  lv_qrcode_update(lv_obj_t *qrcode, const void *data, uint32_t data_len);
void      lv_qrcode_delete(lv_obj_t *qrcode);
```

Usado por `UNIHIKER_K10::canvasDrawCode(code)` com tamanho 230 px.

## 3. `esp_code_scanner` (decoder QR/Barcode pela câmera)

Header esperado: `esp_code_scanner.h` (faz parte da camada precompilada
DFRobot, baseada no projeto `esp32-camera` + `quirc`).

Função-chave (deduzida pelo uso em `who_ai_utils`):

```cpp
typedef struct {
  char *content;
  int   length;
} esp_code_t;

esp_code_t* esp_code_scanner_scan_image(camera_fb_t *fb);
void        esp_code_scanner_free(esp_code_t *code);
```

Acionado pela `task_code_recognition` em `AIRecognition` quando o modo é `Code`;
o resultado vai para `aiData->codeData` e é exposto via
`AIRecognition::getQrCodeContent()`.

## 4. Para reimplementar

- Para fontes CJK sem chip externo, embarcar `noto-sans-cjk` num `lv_font_t`
  estático e remover o adapter — porém custa muitos MB de flash.
- Para QR scanning sem `esp_code_scanner`, usar `quirc` direto (open source,
  trivial portar): receber frame Y (grayscale), `quirc_decode`.
