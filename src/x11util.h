#pragma once
#include <cstdint>
// X11 utility functions - pure C interface, no Qt, no X11 macros in this header

void x11_set_dock_type(unsigned long winId);
void x11_clear_transient_for(unsigned long winId);
void x11_set_strut(unsigned long winId, int screenX, int screenY, int screenW, int screenH,
                   int position, int thickness);
void x11_clear_strut(unsigned long winId);
void x11_activate_window(unsigned long winId);
void x11_close_window(unsigned long winId);
int  x11_get_num_desktops();
int  x11_get_current_desktop();
void x11_set_current_desktop(int index);

struct X11WindowInfo {
    unsigned long wid;
    char wmClass[256];
    char wmName[256];
    bool valid;
};

// Returns number of windows filled into out[] (max maxOut)
int  x11_get_client_list(X11WindowInfo* out, int maxOut);
unsigned long x11_get_active_window();
bool x11_get_window_title(unsigned long wid, char* out, int maxLen);

// ── Vigilador de cambios del WM (eventos push en vez de polling) ─────────────
// Abre una conexión X dedicada que selecciona PropertyChangeMask en la root
// para recibir cambios de _NET_CLIENT_LIST / _NET_ACTIVE_WINDOW en cuanto
// ocurren (el dock deja de sondear cada N ms).
void* x11_wm_watch_open();
void  x11_wm_watch_close(void* handle);
int   x11_wm_watch_fd(void* handle);   // fd para QSocketNotifier
unsigned long x11_atom(const char* name);  // átomo internado (igual en todo el servidor)
// Drena eventos pendientes de la conexión de vigilancia y devuelve 1 si cambió
// la lista de clientes o la ventana activa, 0 en otro caso.
int   x11_wm_watch_dispatch(void* handle, unsigned long atomClientes,
                            unsigned long atomActiva);
