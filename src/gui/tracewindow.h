#ifndef TraceWindow_H
#define TraceWindow_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>

#include "timerwindow.h"


class TraceWindow : public QMainWindow
{
    Q_OBJECT

public:
    TraceWindow( const TimerWindow *parent );
    ~TraceWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:


private:
    const TimerWindow *parent;

};


#endif
