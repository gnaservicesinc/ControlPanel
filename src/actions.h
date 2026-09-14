#pragma once
#include "model.h"
#include <QObject>

namespace cp {
struct ActionResult { bool ok; QString message; };
ActionResult incrementFile(const QString &path, const QString &increment);

class ActionRunner : public QObject {
    Q_OBJECT
public:
    explicit ActionRunner(QObject *parent = nullptr) : QObject(parent) {}
    void execute(const Button &button, const QString &baseDirectory);
    void stopAll();
    int activeCount() const { return active; }
signals:
    void started(const QString &name);
    void finished(const QString &name, bool ok, const QString &message);
private:
    int active = 0;
};
}
