#ifndef TraceWindow_H
#define TraceWindow_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>

#include <array>

#include "timerwindow.h"


class CurrentTimeLineClass;


class TraceWindow : public QMainWindow
{
    Q_OBJECT

public:
    TraceWindow( const TimerWindow *parent );
    ~TraceWindow();

protected:
    void closeEvent(QCloseEvent *event);
    void moveTimelineWindow( int i, double t );

private slots:
    void reset();
    void resizeEvent( QResizeEvent *e );
    void resizeDone();
    void selectProcessor( int );
    void selectThread( int );
    void resolutionChanged();

private:
    enum UpdateType: uint16_t { null=0, time=0x1, proc=0x2, all=0xFFFF };
    void createToolBars();
    void updateDisplay(UpdateType);
    void updateTimeline();
    void updateTimers();
    void updateMemory();

    QLabel *timeline;
    QGridLayout *timerGrid;
    QScrollArea *timerArea;
    QWidget *memory;
    QTimer resizeTimer;
    QToolButton *processorButton;
    QToolButton *threadButton;
    QLineEdit *resolutionBox;
    std::vector<std::shared_ptr<QLabel>> timerLabels;
    std::vector<std::shared_ptr<QLabel>> timerPlots;
    std::vector<std::shared_ptr<QPixmap>> timerPixelMap;

    std::shared_ptr<CurrentTimeLineClass> timelineBoundaries[2];

private:
    std::vector<std::shared_ptr<TimerTimeline>> getTraceData(const std::array<double,2>& t) const;
    const TimerWindow *parent;
    const int N_procs;
    const int N_threads;
    const std::array<double,2> t_global;
    std::array<double,2> t_current;
    int resolution;
    int selected_rank;
    int selected_thread;
    std::map<id_struct,uint64_t> idRgbMap;
    std::shared_ptr<QPixmap> timelinePixelMap;

private:
    static std::array<double,2> getGlobalTime( const std::vector<TimerResults>& timers );

friend class CurrentTimeLineClass;
};


#endif
