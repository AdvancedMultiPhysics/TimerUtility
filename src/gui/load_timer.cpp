#include <QApplication>
#include "timerwindow.h"
#include "ProfilerApp.h"

int main(int argc, char *argv[])
{
    PROFILE_ENABLE();
    Q_INIT_RESOURCE(application);

    QApplication app(argc, argv);
    app.setOrganizationName("");
    app.setApplicationName("load_timer");
    TimerWindow mainWin;
#if defined(Q_OS_SYMBIAN)
    mainWin.showMaximized();
#else
    mainWin.show();
#endif
    int rtn = app.exec();
    return rtn;
}
