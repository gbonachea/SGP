#include "SystemTrayArea.h"
#include <QPainter>
#include <QDateTime>
#include <QTimer>
#include <QFont>
#include <QFontMetrics>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

SystemTrayArea::SystemTrayArea(DockPosition position, QWidget* parent)
    : QWidget(parent)
    , m_position(position)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        updateClock();
        update();
    });
    m_clockTimer->start();

    updateClock();
    updateBattery();
    updateNetwork();
}

void SystemTrayArea::setPosition(DockPosition pos) {
    m_position = pos;
    updateGeometry();
    update();
}

QSize SystemTrayArea::sizeHint() const {
    bool horizontal = (m_position == DockPosition::Abajo ||
                       m_position == DockPosition::Arriba);
    if (horizontal) return QSize(90, 60);
    else            return QSize(60, 80);
}

void SystemTrayArea::updateClock() {
    QDateTime now = QDateTime::currentDateTime();
    m_timeStr = now.toString("HH:mm");
    m_dateStr = now.toString("ddd d");
}

void SystemTrayArea::updateBattery() {
    // Query UPower via D-Bus
    QDBusInterface upower("org.freedesktop.UPower",
                          "/org/freedesktop/UPower",
                          "org.freedesktop.UPower",
                          QDBusConnection::systemBus());
    if (!upower.isValid()) {
        m_batteryPercent = -1;
        return;
    }

    QDBusReply<QList<QDBusObjectPath>> devices = upower.call("EnumerateDevices");
    if (!devices.isValid()) return;

    for (const QDBusObjectPath& path : devices.value()) {
        QDBusInterface dev("org.freedesktop.UPower",
                           path.path(),
                           "org.freedesktop.UPower.Device",
                           QDBusConnection::systemBus());
        if (!dev.isValid()) continue;

        int type = dev.property("Type").toInt();
        if (type != 2) continue;   // 2 = Battery

        m_batteryPercent = (int)dev.property("Percentage").toDouble();
        int state = dev.property("State").toInt();
        m_charging = (state == 1 || state == 5); // Charging or PendingCharge
        break;
    }
}

void SystemTrayArea::updateNetwork() {
    // Query NetworkManager via D-Bus
    QDBusInterface nm("org.freedesktop.NetworkManager",
                      "/org/freedesktop/NetworkManager",
                      "org.freedesktop.NetworkManager",
                      QDBusConnection::systemBus());
    if (!nm.isValid()) return;

    int state = nm.property("State").toInt();
    // 70 = NM_STATE_CONNECTED_GLOBAL
    m_networkConnected = (state >= 60);
}

void SystemTrayArea::timerEvent(QTimerEvent*) {
    updateClock();
    update();
}

void SystemTrayArea::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    bool horizontal = (m_position == DockPosition::Abajo ||
                       m_position == DockPosition::Arriba);

    int cx = width() / 2;
    int cy = height() / 2;

    // Clock
    {
        QFont timeFont("Helvetica Neue", 13, QFont::Medium);
        if (!QFont(timeFont).exactMatch())
            timeFont.setFamily("DejaVu Sans");

        p.setFont(timeFont);
        p.setPen(Qt::white);

        QFontMetrics fm(timeFont);
        QRect timeRect = fm.boundingRect(m_timeStr);

        if (horizontal) {
            // Time on top, date below
            p.drawText(QRect(0, cy - timeRect.height() - 2, width(), timeRect.height() + 4),
                       Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);

            QFont dateFont("Helvetica Neue", 9);
            p.setFont(dateFont);
            p.setOpacity(0.65);
            p.drawText(QRect(0, cy + 4, width(), 20),
                       Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);
            p.setOpacity(1.0);
        } else {
            // Vertical: time then date stacked
            p.drawText(QRect(0, 8, width(), 24),
                       Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);
            QFont dateFont("Helvetica Neue", 8);
            p.setFont(dateFont);
            p.setOpacity(0.65);
            p.drawText(QRect(0, 30, width(), 18),
                       Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);
            p.setOpacity(1.0);
        }
    }

    // Battery indicator (if present)
    if (m_batteryPercent >= 0) {
        int bx, by, bw, bh;
        if (horizontal) {
            bw = 22; bh = 10;
            bx = width() / 2 - bw / 2;
            by = height() - bh - 4;
        } else {
            bw = 22; bh = 10;
            bx = width() / 2 - bw / 2;
            by = height() - bh - 8;
        }

        // Battery body
        p.setPen(QPen(QColor(255,255,255,160), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(bx, by, bw - 2, bh, 2, 2);
        // Terminal
        p.drawRect(bx + bw - 2, by + 2, 2, bh - 4);

        // Fill
        int fillW = qMax(2, (int)((bw - 4) * m_batteryPercent / 100.0));
        QColor fillColor = m_batteryPercent > 20 ? QColor(100, 230, 100, 200)
                                                  : QColor(255, 80, 80, 200);
        if (m_charging) fillColor = QColor(100, 180, 255, 200);
        p.setBrush(fillColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(bx + 1, by + 1, fillW, bh - 2, 1.5, 1.5);
    }
}
