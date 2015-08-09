#include <QtGui>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QHBoxLayout>
#include <QCloseEvent>

#include <memory>
#include <vector>
#include <set>
#include <limits>
#include <sstream>

#include "tracewindow.h"


/***********************************************************************
* Constructor/destructor                                               *
***********************************************************************/
TraceWindow::TraceWindow( const TimerWindow *parent_ ):
    parent(parent_)
{
    QWidget::setWindowTitle(QString("Trace results: ").append(parent->windowTitle()));
}
TraceWindow::~TraceWindow()
{
}
void TraceWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}


