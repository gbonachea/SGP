#pragma once
#include <QObject>
#include <QString>
#include <QIcon>
#include <QList>
#include <QMap>

struct InfoApp {
    QString nombre;
    QString archivoDesktop;
    QString comandoExec;
    QString nombreIcono;
    QIcon   icono;
    QString wmClass;
    QString categorias;   // raw "Categories=" del .desktop, para agrupar por categoría
    bool    estaAnclada  = false;
    bool    estaEjecutando = false;
    QList<quint64> ventanas;
};

class ApplicationScanner : public QObject {
    Q_OBJECT
public:
    explicit ApplicationScanner(QObject* parent = nullptr);
    void escanear();                      // escaneo completo (bajo demanda)
    QList<InfoApp> todasLasApps();        // dispara el escaneo la primera vez
    InfoApp buscarPorWmClass(const QString& wm);
    InfoApp buscarPorDesktop(const QString& df) const;

signals:
    void appsActualizadas();

private:
    void asegurarEscaneo();
    InfoApp parsearDesktop(const QString& path) const;
    QIcon   cargarIcono(const QString& nombre) const;
    QString limpiarExec(const QString& exec) const;

    QList<InfoApp>         m_apps;
    QMap<QString,InfoApp>  m_porWmClass;
    QMap<QString,InfoApp>  m_porDesktop;
    bool                   m_escaneado = false;
};
