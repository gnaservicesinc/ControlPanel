#include "application.h"
#include <QFileOpenEvent>

namespace cp {
bool Application::event(QEvent *event) {
    if (event->type() == QEvent::FileOpen) {
        if (openFile) openFile(static_cast<QFileOpenEvent *>(event)->file());
        return true;
    }
    return QApplication::event(event);
}
}
