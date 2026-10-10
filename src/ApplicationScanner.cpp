#include "ApplicationScanner.h"
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDebug>
#include <QLocale>
#include <algorithm>

ApplicationScanner::ApplicationScanner(QObject* parent) : QObject(parent) {
    // El escaneo completo (.desktop + iconos) se hace bajo demanda la primera
    // vez que hace falta el catálogo (abrir menú, emparejar ventanas, etc.):
    // el arranque del dock no se carga con cientos de archivos.
}

void ApplicationScanner::asegurarEscaneo() {
    if (!m_escaneado) {
        escanear();
        m_escaneado = true;
    }
}

void ApplicationScanner::escanear() {
    m_apps.clear();
    m_porWmClass.clear();
    m_porDesktop.clear();

    QStringList directorios = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    QSet<QString> vistos;

    for (const QString& dir : directorios) {
        QDir appsDir(dir + "/applications");
        if (!appsDir.exists()) continue;

        // Recursivo para subdirectorios como /usr/share/applications/kde/
        QStringList archivos = appsDir.entryList({"*.desktop"}, QDir::Files);
        for (const QString& f : archivos) {
            if (vistos.contains(f)) continue;
            vistos.insert(f);

            InfoApp info = parsearDesktop(appsDir.absoluteFilePath(f));
            if (info.nombre.isEmpty() || info.comandoExec.isEmpty()) continue;

            m_apps.append(info);
            m_porDesktop[info.archivoDesktop] = info;
            if (!info.wmClass.isEmpty())
                m_porWmClass[info.wmClass.toLower()] = info;
            // También indexar por nombre del ejecutable
            QString exe = info.comandoExec.split(' ').first();
            QString base = QFileInfo(exe).baseName().toLower();
            if (!base.isEmpty() && !m_porWmClass.contains(base))
                m_porWmClass[base] = info;
        }
    }

    std::sort(m_apps.begin(), m_apps.end(), [](const InfoApp& a, const InfoApp& b){
        return a.nombre.toLower() < b.nombre.toLower();
    });

    m_escaneado = true;
    emit appsActualizadas();
}

InfoApp ApplicationScanner::parsearDesktop(const QString& path) const {
    InfoApp info;
    QSettings s(path, QSettings::IniFormat);
    s.beginGroup("Desktop Entry");

    if (s.value("NoDisplay", false).toBool()) return {};
    if (s.value("Hidden", false).toBool()) return {};
    if (s.value("Type", "").toString() != "Application") return {};

    info.archivoDesktop = path;
    info.nombre         = s.value("Name", "").toString();
    info.comandoExec    = limpiarExec(s.value("Exec", "").toString());
    info.nombreIcono    = s.value("Icon", "").toString();
    info.wmClass        = s.value("StartupWMClass", "").toString();
    info.categorias     = s.value("Categories", "").toString();

    // Nombre localizado
    QString locale = QLocale::system().name();                    // ej: "pt_BR"
    QString lang   = locale.split('_').first();                   // ej: "pt"
    for (const QString& key : {QString("Name[%1]").arg(locale),
                                QString("Name[%1]").arg(lang)}) {
        QString loc = s.value(key, "").toString();
        if (!loc.isEmpty()) { info.nombre = loc; break; }
    }

    info.icono = cargarIcono(info.nombreIcono);
    if (info.icono.isNull())
        info.icono = QIcon::fromTheme("application-x-executable");

    s.endGroup();
    return info;
}

QIcon ApplicationScanner::cargarIcono(const QString& nombre) const {
    if (nombre.isEmpty()) return {};
    if (nombre.startsWith('/')) return QIcon(nombre);

    // Buscar en tema primero
    QIcon icon = QIcon::fromTheme(nombre);
    if (!icon.isNull()) return icon;

    // Buscar en rutas estándar
    QStringList exts  = {".png", ".svg", ".xpm"};
    QStringList sizes = {"48x48", "32x32", "64x64", "128x128", "scalable", "22x22"};
    QStringList cats  = {"apps", "applications"};
    QStringList bases = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    bases << "/usr/share" << "/usr/local/share";

    for (const QString& base : bases) {
        // hicolor theme
        for (const QString& sz : sizes)
            for (const QString& cat : cats)
                for (const QString& ext : exts) {
                    QString p = QString("%1/icons/hicolor/%2/%3/%4%5").arg(base,sz,cat,nombre,ext);
                    if (QFile::exists(p)) return QIcon(p);
                }
        // pixmaps fallback
        for (const QString& ext : exts) {
            QString p = QString("%1/pixmaps/%2%3").arg(base, nombre, ext);
            if (QFile::exists(p)) return QIcon(p);
        }
    }
    return {};
}

QString ApplicationScanner::limpiarExec(const QString& exec) const {
    QString r = exec;
    r.remove(QRegularExpression("%[fFuUdDnNickvm]\\s*"));
    return r.trimmed();
}

InfoApp ApplicationScanner::buscarPorWmClass(const QString& wm) {
    asegurarEscaneo();
    QString k = wm.toLower();
    if (m_porWmClass.contains(k)) return m_porWmClass[k];
    // búsqueda parcial
    for (const InfoApp& a : m_apps)
        if (a.nombre.toLower() == k || a.wmClass.toLower() == k) return a;
    return {};
}

InfoApp ApplicationScanner::buscarPorDesktop(const QString& df) const {
    // Sin escaneo global pendiente: basta con parsear ese archivo (ancladas).
    if (!m_escaneado) return parsearDesktop(df);
    return m_porDesktop.value(df, InfoApp{});
}

QList<InfoApp> ApplicationScanner::todasLasApps() {
    asegurarEscaneo();
    return m_apps;
}
