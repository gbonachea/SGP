#pragma once
#include <QWidget>
#include <QList>
#include <QTimer>
#include <QLabel>
#include "DockConfig.h"

/**
 * SystemTrayArea — an optional widget that can be embedded in the dock
 * or floated as a separate panel showing:
 *   - System clock
 *   - Battery level (if available via UPower D-Bus)
 *   - Network status (if available via NM D-Bus)
 *   - Volume indicator (via PulseAudio / PipeWire D-Bus)
 *
 * This widget is shown as a compact area at the end of the dock
 * (right side for horizontal, bottom for vertical).
 */
class SystemTrayArea : public QWidget {
    Q_OBJECT
public:
    explicit SystemTrayArea(DockPosition position, QWidget* parent = nullptr);

    void setPosition(DockPosition pos);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;
    void timerEvent(QTimerEvent* e) override;

private:
    void updateClock();
    void updateBattery();
    void updateNetwork();

    DockPosition m_position;
    QTimer*      m_clockTimer;

    QString      m_timeStr;
    QString      m_dateStr;
    int          m_batteryPercent = -1;   // -1 = no battery
    bool         m_charging = false;
    bool         m_networkConnected = false;
    QString      m_networkName;
};
