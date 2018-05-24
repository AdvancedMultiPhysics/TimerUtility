#ifndef TraceWindow_H
#define TraceWindow_H

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
class DrawVerticalLineClass;
class QLabelMouse;


class TraceWindow : public ThreadedSlotsClass
{
    Q_OBJECT

public:
    explicit TraceWindow( const TimerWindow *parent );
    TraceWindow() = delete;
    TraceWindow( const TraceWindow& ) = delete;
    TraceWindow& operator=( const TraceWindow& ) = delete;
    virtual ~TraceWindow();

protected:
    void closeEvent( QCloseEvent *event );
    void moveTimelineWindow( int i, double t );

private slots:
    void reset();
    void resetZoom();
    void resizeEvent( QResizeEvent *e );
    void resizeDone();
    void selectProcessor( int );
    void selectThread( int );
    void resolutionChanged();

private:
    enum UpdateType : uint16_t { null = 0, time = 0x1, proc = 0x2, header = 0x4, all = 0xFFFF };
    void createToolBars();
    void updateDisplay( UpdateType );
    void updateTimeline();
    void updateTimers();
    void updateMemory();
    void resizeMemory();

    QLabel *timeline;
    QSplitterGrid *timerGrid;
    QWidget *memory;
    QTimer resizeTimer;
    QToolButton *processorButton;
    QToolButton *threadButton;
    QLineEdit *resolutionBox;
    QMenu *processorButtonMenu;
    QMenu *threadButtonMenu;
    std::vector<QLabel *> timerLabels;
    std::vector<QLabelMouse *> timerPlots;
    std::shared_ptr<CurrentTimeLineClass> timelineBoundaries[2];

private:
    std::vector<std::shared_ptr<TimerTimeline>> getTraceData(
        const std::array<double, 2> &t ) const;
    const TimerWindow *parent;
    const int N_procs;
    const int N_threads;
    const std::array<double, 2> t_global;
    std::array<double, 2> t_current;
    int resolution;
    int selected_rank;
    int selected_thread;
    std::map<id_struct, uint64_t> idRgbMap;
    std::shared_ptr<QPixmap> timelinePixelMap;

    bool traceZoomActive;
    std::array<double, 2> t_zoom;
    int traceZoomLastPos;
    std::shared_ptr<DrawVerticalLineClass> zoomBoundaries[2];

private:
    static std::array<double, 2> getGlobalTime( const std::vector<TimerResults> &timers );

protected:
    void traceMousePressEvent( QMouseEvent *event );
    void traceMouseMoveEvent( QMouseEvent *event );
    void traceMouseReleaseEvent( QMouseEvent *event );

    friend class CurrentTimeLineClass;
    friend class QLabelMouse;

    // Data for unit testing
public:
    int runUnitTests();

private:
    void callDefaultTests();
};


#endif
