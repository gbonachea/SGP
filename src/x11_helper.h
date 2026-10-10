#pragma once
// Este header DEBE incluirse DESPUÉS de todos los headers Qt
// X11 define macros (None, Bool, Status, etc.) que rompen Qt si se incluye antes.
//
// Uso correcto en .cpp:
//   #include <QtWidgets/...>   ← primero todos los Qt
//   #include "x11_helper.h"   ← al final del bloque de includes

// Undefine macros conflictivos de X11 que rompen Qt
#ifdef None
#  undef None
#endif
#ifdef Bool
#  undef Bool
#endif
#ifdef Status
#  undef Status
#endif
#ifdef Unsorted
#  undef Unsorted
#endif
#ifdef True
#  undef True
#endif
#ifdef False
#  undef False
#endif
#ifdef KeyPress
#  undef KeyPress
#endif
#ifdef KeyRelease
#  undef KeyRelease
#endif
#ifdef FocusIn
#  undef FocusIn
#endif
#ifdef FocusOut
#  undef FocusOut
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <qpa/qplatformnativeinterface.h>
#include <QGuiApplication>
#include <QScreen>

inline Display* x11Display() {
    auto* ni = QGuiApplication::platformNativeInterface();
    if (!ni) return nullptr;
    return reinterpret_cast<Display*>(
        ni->nativeResourceForScreen("display", QGuiApplication::primaryScreen()));
}
