#pragma once
#include <QString>
#include <QColor>

enum class DockPosition { Abajo, Arriba, Izquierda, Derecha };
enum class DockComportamiento { SiempreVisible, AutoOcultar };

struct DockConfig {
    DockPosition       posicion           = DockPosition::Abajo;
    DockComportamiento comportamiento      = DockComportamiento::SiempreVisible;
    int                tamanoIcono        = 32;
    bool               magnificacion      = true;
    int                multiplicadorMag   = 160;   // % del tamaño base
    bool               fondoTranslucido   = true;
    double             opacidadFondo      = 0.80;
    bool               mostrarIndicadores = true;
    bool               mostrarEtiquetas   = true;
    bool               rebotarAlAbrir     = true;
    int                indiceMonitor      = 0;
    int                radioEsquina       = 16;
    int                espaciadoItems     = 6;
    int                padding            = 10;
    int                margen             = 4;

    // Modo panel: estira el dock a todo el ancho (o alto) del borde de la
    // pantalla, repartiendo los iconos de punta a punta y manteniendo los
    // bordes redondeados. Activo por defecto.
    bool               modoPanel          = true;

    // Altura (o grosor) del panel en modo panel. 42 px es la medida cómoda:
    // los iconos de 32 px quedan con margen, los indicadores de ejecución
    // caben bajo el icono y el área táctil supera los 40 px recomendados.
    int                altoPanel          = 42;

    // Bandeja del sistema X11 (XEmbed) integrada en el dock
    bool               mostrarTray        = true;
    int                tamanoTray         = 24;
};
