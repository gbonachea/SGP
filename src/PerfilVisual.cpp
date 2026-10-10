#include "PerfilVisual.h"

namespace {
bool g_ligero = false;
}

namespace PerfilVisual {

void setLigero(bool on) { g_ligero = on; }

bool ligero() { return g_ligero; }

QColor colorFondoLigero() { return QColor(30, 30, 30); }

} // namespace PerfilVisual
