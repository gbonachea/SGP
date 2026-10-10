#include "AppLauncher.h"
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

AppLauncher::AppLauncher(QObject* parent)
    : QObject(parent)
{}

qint64 AppLauncher::launch(const QString& execCommand, const QString& workingDir) {
    if (execCommand.isEmpty()) return -1;

    // Clean up field codes (%f, %F, %u, %U, etc.)
    QString cleaned = execCommand;
    cleaned.remove(QRegularExpression("%[fFuUdDnNickvm]\\s*"));
    cleaned = cleaned.trimmed();

    QStringList parts = cleaned.split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return -1;

    QString program = parts.takeFirst();
    QStringList args = parts;

    QProcess* proc = new QProcess(this);
    if (!workingDir.isEmpty()) proc->setWorkingDirectory(workingDir);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AppLauncher::onProcessFinished);

    proc->start(program, args);

    if (!proc->waitForStarted(3000)) {
        qWarning() << "AppLauncher: failed to start" << program;
        proc->deleteLater();
        return -1;
    }

    qint64 pid = proc->processId();
    m_processes[proc] = pid;
    m_commands[proc] = execCommand;

    emit appLaunched(pid, execCommand);
    return pid;
}

bool AppLauncher::launchDetached(const QString& execCommand) {
    QString cleaned = execCommand;
    cleaned.remove(QRegularExpression("%[fFuUdDnNickvm]\\s*"));
    cleaned = cleaned.trimmed();
    return QProcess::startDetached("/bin/sh", {"-c", cleaned});
}

void AppLauncher::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    QProcess* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;

    qint64 pid = m_processes.value(proc, -1);
    if (pid != -1) emit appFinished(pid, exitCode);

    m_processes.remove(proc);
    m_commands.remove(proc);
    proc->deleteLater();
}
