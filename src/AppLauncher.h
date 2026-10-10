#pragma once
#include <QObject>
#include <QString>
#include <QProcess>
#include <QMap>

/**
 * AppLauncher — responsible for launching applications and tracking
 * their PIDs so the dock can associate windows with dock items.
 */
class AppLauncher : public QObject {
    Q_OBJECT
public:
    explicit AppLauncher(QObject* parent = nullptr);

    /**
     * Launch an application from its Exec= command.
     * Returns the PID of the launched process, or -1 on failure.
     */
    qint64 launch(const QString& execCommand, const QString& workingDir = QString());

    /**
     * Detach-launch (fire and forget). Does not track PID.
     */
    static bool launchDetached(const QString& execCommand);

signals:
    void appLaunched(qint64 pid, const QString& execCommand);
    void appFinished(qint64 pid, int exitCode);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QMap<QProcess*, qint64> m_processes;  // process -> pid
    QMap<QProcess*, QString> m_commands;  // process -> exec command
};
