#pragma once

#include <QColor>

// ─── Perfil visual de emergencia ("--ligero") ────────────────────────────────
// Agrupa el "grupo B" de optimizaciones: las que SÍ tocan la estética porque
// la elegancia del dock se apoya en píxeles caros para la GPU.
//
// Está APAGADO por defecto: sin el flag el dock se ve exactamente igual que
// siempre. Se activa voluntariamente en equipos donde la GPU realmente no da:
//   - transparencia / esquinas redondeadas / sombras y glow → fondo plano opaco
//   - magnificación limitada a ~30 fps
//
// Activación:
//   sgp --ligero        (o -l)
//   SGP_LIGERO=1 sgp    (útil desde lanzadores/autostart)
//
// Es un perfil de arranque: no se guarda en sgp.conf ni se mezcla con la
// configuración persistente del usuario.

namespace PerfilVisual {

// Activa o desactiva el perfil. Se fija una sola vez al arrancar (main.cpp),
// antes de crear cualquier widget.
void setLigero(bool on);

// ¿Está activo el perfil de emergencia?
bool ligero();

// Intervalo objetivo de la magnificación en modo ligero (~30 fps).
inline constexpr int MAG_INTERVALO_MS = 33;

// Color de fondo plano del panel en modo ligero. Los items del dock son
// ventanas hijas: al desactivar la transparencia también deben ser opacas y
// rellenar su fondo con este mismo color para integrarse con el panel.
QColor colorFondoLigero();

} // namespace PerfilVisual
