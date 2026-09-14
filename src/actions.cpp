#include "actions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <cmath>
#include <memory>

namespace cp {
void ActionRunner::stopAll() {
    for (auto *process : findChildren<QProcess *>()) {
        if (process->state() == QProcess::NotRunning) continue;
        process->terminate();
        QTimer::singleShot(1500, process, [process] {
            if (process->state() != QProcess::NotRunning) process->kill();
        });
    }
}

ActionResult incrementFile(const QString &path, const QString &increment) {
    double delta = 1;
    if (!increment.isEmpty() && !parseNumber(increment, &delta)) return {false, "The increment is not a finite number."};
    // Resolve symlinks before locking so different panel paths share one lock.
    QFileInfo info(path);
    if (info.isSymLink() && !info.exists()) return {false, "The counter is a broken symbolic link."};
    const QString target = info.exists() ? info.canonicalFilePath()
        : QDir(QFileInfo(info.absolutePath()).canonicalFilePath()).absoluteFilePath(info.fileName());
    if (!QFileInfo(info.absolutePath()).isDir()) return {false, "The destination folder does not exist."};
    if (info.exists() && !info.isFile()) return {false, "The counter path must be a regular file."};
    QLockFile lock(target + ".controlpanel.lock");
    if (!lock.tryLock(0)) return {false, "The counter is busy or its folder is not writable. Try again."};
    double current = 0;
    QFile input(target);
    if (input.open(QIODevice::ReadOnly)) {
        if (input.size() > 64 * 1024) return {false, "Counter files must be no larger than 64 KiB."};
        // The first whitespace-separated number is the stored value; other text is ignored.
        // The size guard prevents silently replacing an unrelated large file.
        const auto text = QString::fromUtf8(input.read(64 * 1024));
        static const QRegularExpression firstNumber(R"((?:^|\s)([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)(?=\s|$))");
        const auto match = firstNumber.match(text);
        if (match.hasMatch() && !parseNumber(match.captured(1), &current))
            return {false, "The stored number is outside the supported numeric range."};
        input.close();
    }
    const double result = current + delta;
    if (!std::isfinite(result)) return {false, "The resulting counter would overflow."};
    if (delta != 0 && result == current) return {false, "The increment is too small for this counter's numeric precision."};
    const auto number = QString::number(result, 'g', 17);
    const auto bytes = (number + '\n').toUtf8();
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
        return {false, "Could not write counter: " + output.errorString()};
    return {true, "Counter is now " + number + "."};
}

void ActionRunner::execute(const Button &button, const QString &baseDirectory) {
    const auto validation = validate({"Action", {{"Action", {button}}}});
    if (!validation.isEmpty()) { emit finished(button.name, false, validation); return; }
    if (active >= 16) { emit finished(button.name, false, "There are already 16 running actions. Wait for one to finish."); return; }
    emit started(button.name);
    const auto path = resolvePath(button.path, baseDirectory);
    if (button.action == Action::StoreIncrement) {
        const auto result = incrementFile(path, button.value);
        emit finished(button.name, result.ok, result.message);
        return;
    }
    QStringList args;
    QString program;
    if (button.action == Action::Touch) {
        program = "/usr/bin/touch";
        args = QStringList{path}; // Always absolute, so filenames cannot become command options.
    } else {
        const QFileInfo file(path);
        if (!file.isFile() || !file.isExecutable()) {
            emit finished(button.name, false, "Choose an executable file. Scripts need a shebang (such as #!/bin/sh) and execute permission.");
            return;
        }
        QString error;
        if (!splitArguments(button.args, &args, &error)) { emit finished(button.name, false, error); return; }
        program = path;
    }
    auto *process = new QProcess(this);
    process->setWorkingDirectory(baseDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    // Finder and Terminal launches should resolve script tools consistently.
    environment.insert("PATH", "/usr/bin:/bin:/usr/sbin:/sbin");
    for (const auto &key : environment.keys())
        if (key.startsWith("DYLD_") || key.startsWith("QT_") || key.startsWith("QML_")) environment.remove(key);
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    auto output = std::make_shared<QByteArray>();
    auto done = std::make_shared<bool>(false);
    const auto collect = [process, output] {
        output->append(process->readAllStandardOutput());
        if (output->size() > 16384) *output = output->right(16384);
    };
    connect(process, &QProcess::readyReadStandardOutput, this, collect);
    const auto complete = [this, process, output, done, collect, name = button.name](bool ok, const QString &message) {
        if (*done) return;
        *done = true;
        collect();
        --active;
        QString details = message;
        if (!output->trimmed().isEmpty()) details += "\n" + QString::fromUtf8(*output).trimmed();
        emit finished(name, ok, details);
        process->deleteLater();
    };
    connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(false, "Could not start: " + process->errorString());
    });
    connect(process, &QProcess::finished, this, [complete](int code, QProcess::ExitStatus status) {
        const bool ok = status == QProcess::NormalExit && code == 0;
        complete(ok, status == QProcess::CrashExit ? "The process crashed." : QString("Finished with exit code %1.").arg(code));
    });
    ++active;
    process->start(program, args);
    process->closeWriteChannel(); // Actions are noninteractive; stdin must not wait forever.
}
}
