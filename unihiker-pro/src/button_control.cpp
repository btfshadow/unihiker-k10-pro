#include "button_control.h"
#include "log.h"

namespace unihiker_pro {
namespace {
ButtonControlMapping g_btn_map;
ButtonControlTheme g_btn_theme;

void btn_a_fast() {
  LOG_INFO("[button_control] Botão A pressionado (rápido)");
  if (g_btn_map.a_fast.fn) g_btn_map.a_fast.fn();
}
void btn_a_long() {
  LOG_INFO("[button_control] Botão A pressionado (longo)");
  if (g_btn_map.a_long.fn) g_btn_map.a_long.fn();
}
void btn_b_fast() {
  LOG_INFO("[button_control] Botão B pressionado (rápido)");
  if (g_btn_map.b_fast.fn) g_btn_map.b_fast.fn();
}
void btn_b_long() {
  LOG_INFO("[button_control] Botão B pressionado (longo)");
  if (g_btn_map.b_long.fn) g_btn_map.b_long.fn();
}
void btn_ab_long() {
  LOG_INFO("[button_control] Botões AB pressionados (longo)");
  if (g_btn_map.ab_long.fn) g_btn_map.ab_long.fn();
}

void render_button_legends(DisplayService& display) {
  String legend;
  // Monta legenda: A | B | AB
  if (g_btn_map.a_fast.label.length()) {
    legend += "A: "; legend += g_btn_map.a_fast.label; legend += "  ";
  }
  if (g_btn_map.a_long.label.length()) {
    legend += "A⏱: "; legend += g_btn_map.a_long.label; legend += "  ";
  }
  if (g_btn_map.b_fast.label.length()) {
    legend += "B: "; legend += g_btn_map.b_fast.label; legend += "  ";
  }
  if (g_btn_map.b_long.label.length()) {
    legend += "B⏱: "; legend += g_btn_map.b_long.label; legend += "  ";
  }
  if (g_btn_map.ab_long.label.length()) {
    legend += "AB⏱: "; legend += g_btn_map.ab_long.label;
  }
  // Limpa e desenha
  display.clearRow(7);
  display.setFontSize(g_btn_theme.font);
  display.textAt(legend, 10, g_btn_theme.y, g_btn_theme.normalColor, 60, true);
  display.update();
}
} // namespace

void button_control(
  InputService& input,
  DisplayService& display,
  const ButtonControlMapping& mapping,
  const ButtonControlTheme& theme
) {
  g_btn_map = mapping;
  g_btn_theme = theme;
  // Registra callbacks para cada botão/ação
  input.onReleaseByDuration(ButtonId::A,
    mapping.a_fast.fn ? btn_a_fast : nullptr,
    mapping.a_long.fn ? btn_a_long : nullptr,
    2000);
  input.onReleaseByDuration(ButtonId::B,
    mapping.b_fast.fn ? btn_b_fast : nullptr,
    mapping.b_long.fn ? btn_b_long : nullptr,
    2000);
  input.onReleaseByDuration(ButtonId::AB,
    nullptr,
    mapping.ab_long.fn ? btn_ab_long : nullptr,
    2000);
  // Renderiza legendas
  render_button_legends(display);
}

} // namespace unihiker_pro
