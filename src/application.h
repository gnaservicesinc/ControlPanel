#pragma once
#include <QApplication>
#include <functional>

namespace cp {
class Application : public QApplication {
public:
    Application(int &argc, char **argv) : QApplication(argc, argv) {}
    std::function<void(const QString &)> openFile;
protected:
    bool event(QEvent *event) override;
};
}
