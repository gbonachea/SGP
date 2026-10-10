#include <QApplication>
#include <QIcon>
#include <QDebug>
#include <QString>
#include <QTextStream>
#include <QSettings>
#include <QStandardPaths>
#include "DockWindow.h"
#include "PerfilVisual.h"

// Preferencia guardada por "Preferencias del Dock" (grupo [Dock] de sgp.conf).
static bool perfilLigeroGuardado() {
    const QString cfgPath =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/sgp/sgp.conf";
    QSettings s(cfgPath, QSettings::IniFormat);
    return s.value("Dock/modoLigero", false).toBool();
}

static void imprimirAyuda() {
    QTextStream out(stdout);
    out << "SGP — Stellar Galaxy Dock\n\n"
        << "Uso: sgp [opciones]\n\n"
        << "Opciones:\n"
        << "  -l, --ligero       Perfil de emergencia para GPU débil: fondo plano\n"
        << "                     opaco sin transparencia, esquinas redondeadas ni\n"
        << "                     sombras/glow, y magnificación a ~30 fps.\n"
        << "      --optimizado   Fuerza el perfil visual completo (vidrio,\n"
        << "                     sombras y magnificación fluida), anulando una\n"
        << "                     preferencia guardada en sentido contrario.\n"
        << "  -h, --ayuda        Muestra esta ayuda y sale.\n\n"
        << "La preferencia se guarda en Preferencias del Dock y se recuerda\n"
        << "entre sesiones. --ligero/--optimizado y SGP_LIGERO=1 la anulan\n"
        << "solo para este arranque.\n";
}

int main(int argc, char* argv[]) {
    // Forzar plataforma XCB para soporte de transparencia
    qputenv("QT_QPA_PLATFORM", "xcb");

    // Perfil visual. Orden de prioridad (de menor a mayor):
    //   1) preferencia guardada en sgp.conf
    //   2) SGP_LIGERO=1 en el entorno
    //   3) flag explícito --ligero/--optimizado en la línea de comandos
    // Se decide antes de crear la ventana para que todos los widgets (panel y
    // popups) nazcan coherentes.
    bool ligero = perfilLigeroGuardado();

    const QByteArray envLigero = qgetenv("SGP_LIGERO");
    if (!envLigero.isEmpty() && envLigero != "0")
        ligero = true;

    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--ligero" || a == "-l") {
            ligero = true;
        } else if (a == "--optimizado" || a == "--normal") {
            ligero = false;
        } else if (a == "--ayuda" || a == "--help" || a == "-h") {
            imprimirAyuda();
            return 0;
        }
    }
    PerfilVisual::setLigero(ligero);

    QApplication app(argc, argv);
    app.setApplicationName("SGP");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("Stellar Galaxy Dock");
    app.setWindowIcon(QIcon::fromTheme("start-here"));

    DockWindow dock;
    dock.show();

    return app.exec();
}
