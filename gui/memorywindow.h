#ifndef MemoryWindow_H
#define MemoryWindow_H

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

#include <array>

#include "QSplitterGrid.h"
#include "ThreadedSlotsClass.h"
#include "timerwindow.h"


class CurrentTimeLineClass;
class QLabelMouse;


class MemoryWindow : public ThreadedSlotsClass
{
    Q_OBJECT

public:
    explicit MemoryWindow( const TimerWindow *parent );
    MemoryWindow()                       = delete;
    MemoryWindow( const MemoryWindow & ) = delete;
    MemoryWindow &operator=( const MemoryWindow & ) = delete;
    virtual ~MemoryWindow();

protected:
    void closeEvent( QCloseEvent *event );
    void moveTimelineWindow( int i, double t );

private slots:
    void reset();
    void resetZoom();
    void resizeEvent( QResizeEvent *e );
    void resizeDone();
    void selectProcessor( int );

private:
    enum UpdateType : uint16_t { null = 0, time = 0x1, proc = 0x2, header = 0x4, all = 0xFFFF };
    void createToolBars();
    void updateDisplay( UpdateType );
    void updateMemory();
    void resizeMemory();

    QWidget *memory;
    QTimer resizeTimer;
    QToolButton *processorButton;
    QMenu *processorButtonMenu;
    std::vector<QLabel *> timerLabels;

private:
    const TimerWindow *parent;
    const int N_procs;
    const std::array<double, 2> t_global;
    std::array<double, 2> t_current;
    int selected_rank;

private:
    static std::array<double, 2> getGlobalTime( const std::vector<MemoryResults> &memory );

protected:
    // Data for unit testing
public:
    int runUnitTests();

private:
    void callDefaultTests();
};


#endif
