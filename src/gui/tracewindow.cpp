#include <QtGui>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QStatusBar>
#include <QAction>
#include <QMenuBar>
#include <QMenu>

#include <memory>
#include <vector>
#include <set>
#include <array>
#include <limits>
#include <sstream>

#include "tracewindow.h"
#include "TableValue.h"
#include "MemoryPlot.h"
#include "colormap.h"


// Class to draw a vertical line
class DrawVerticalLineClass : public QWidget
{
public:
    DrawVerticalLineClass(QWidget* parent, double pos_, int boarder=0 ):
        QWidget(parent), d_parent(parent), d_boarder(boarder), pos(pos_)
    {
        move( pos );
    }
    // Move to the given position
    void move( double pos_ ) {
        pos = pos_;
        pos = std::max(pos,0.0);
        pos = std::min(pos,1.0);
        int x, y, w, h;
        d_parent->rect().getRect(&x,&y,&w,&h);
        setGeometry(QRect(x+w*pos-d_boarder,y,2+2*d_boarder,h));
        this->update();
    }
    // Update
    void redraw( ) {
        move(pos);
    }
    // Draw the rectangle
    void paintEvent(QPaintEvent *) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int x, y, w, h;
        rect().getRect(&x,&y,&w,&h);
        p.fillRect(QRect(x+d_boarder,y,2,h),QBrush(QColor(255,255,255)));
    }
protected:
    QWidget *d_parent;
    int d_boarder;
    double pos;
};


// Class to draw a line for the current time
class CurrentTimeLineClass : public DrawVerticalLineClass
{
public:
    CurrentTimeLineClass(QWidget* parent, TraceWindow *traceWindow, 
        const std::array<double,2>& t_global, int id, double t0 ):
        DrawVerticalLineClass(parent,0,3), d_traceWindow(traceWindow),
        d_t_global(t_global), d_t(t0), pos0(0), d_id(id), start(0), last(0), active(false)
    {
        setMouseTracking(true);
        setTime(t0);
    }
    void setTime( double t ) {
        d_t = t;
        pos0 = (t-d_t_global[0])/(d_t_global[1]-d_t_global[0]);
        move(pos0);
    }
    void mousePressEvent(QMouseEvent *event)
    {
        if (event->button() == Qt::LeftButton) {
            start = event->globalPos().x();
            last = start;
            active = true;
        }
    }
    void mouseMoveEvent(QMouseEvent *event)
    {
        int pos = event->globalPos().x();
        if ( !active || abs(last-pos) < 4 )
            return;
        last = pos;
        double w = d_parent->rect().width();
        double pos2 = pos0 + static_cast<double>(pos-start)/w;
        move(pos2);
    }
    void mouseReleaseEvent(QMouseEvent * event)
    {
        if (event->button() == Qt::LeftButton) {
            int pos = event->globalPos().x();
            double w = d_parent->rect().width();
            double pos2 = pos0 + static_cast<double>(pos-start)/w;
            double t = d_t_global[0] + pos2*(d_t_global[1]-d_t_global[0]);
            t = std::max(t,d_t_global[0]);
            t = std::min(t,d_t_global[1]);
            active = false;
            start=last=0;
            setTime(t);
            d_traceWindow->moveTimelineWindow(d_id,t);
        }
    }
private:
    TraceWindow *d_traceWindow;
    const std::array<double,2> d_t_global;
    double d_t, pos0;
    int d_id;
    int start, last;
    bool active;

};


// Class for QLabel that has mouse movement functions
class QLabelMouse : public QLabel
{
public:
	QLabelMouse( TraceWindow *trace ):
        QLabel(NULL), d_trace(trace), active(false), start(0), last(0) {}
	void mousePressEvent( QMouseEvent * event ) { d_trace->traceMousePressEvent(event); }
	void mouseMoveEvent( QMouseEvent * event  ) { d_trace->traceMouseMoveEvent(event);  }
	void mouseReleaseEvent( QMouseEvent * event ) { d_trace->traceMouseReleaseEvent(event); }
private:
    TraceWindow *d_trace;
    bool active;
    int start, last;
};


// Compute the sum of a vector
template<class TYPE> TYPE sum( const std::vector<TYPE>& x )
{
    TYPE y = 0;
    for (size_t i=0; i<x.size(); i++)
        y += x[i];
    return y;
}


// Function to add an action (with connection) to a menu
#define ADD_MENU_ACTION( menu, string, arg ) \
    do {                                            \
        QAction* action = new QAction(string,this); \
        connect(action, SIGNAL(triggered()), signalMapper, SLOT(map())); \
        signalMapper->setMapping(action,arg);       \
        menu->addAction( action );                  \
    } while(0)                                      \



/***********************************************************************
* Constructor/destructor                                               *
***********************************************************************/
TraceWindow::TraceWindow( const TimerWindow *parent_ ):
    timerGrid(NULL), memory(NULL),
    parent(parent_), N_procs(parent_->N_procs), N_threads(parent_->N_threads),
    t_global(getGlobalTime(parent_->d_data.timers)), 
    resolution(1024), selected_rank(-1), selected_thread(-1)
{
    PROFILE_START("TraceWindow");
    QWidget::setWindowTitle(QString("Trace results: ").append(parent->windowTitle()));
    t_current = t_global;
    resize(1200,800);

    // Create the master timeline
    timeline = new QLabel;
    timeline->setScaledContents(true);
    timeline->setMinimumSize(10,10);

    // Create the timer timelines
    timerGrid = new QGridLayout();
    timerGrid->setSpacing(0);
	timerGrid->setVerticalSpacing(0);
    QWidget *timerGridWidget = new QWidget;
    timerGridWidget->setLayout(timerGrid);
    timerLabels.resize(parent->d_data.timers.size());
    timerPlots.resize(parent->d_data.timers.size());
    for (size_t i=0; i<timerLabels.size(); i++) {
        timerLabels[i].reset(new QLabel());
        timerLabels[i]->setFixedWidth(120);
        timerPlots[i].reset(new QLabelMouse(this));
        timerPlots[i]->setScaledContents(true);
        timerPlots[i]->setMinimumSize(1000,30);
        timerGrid->addWidget(timerLabels[i].get(),i,0);
        timerGrid->addWidget(timerPlots[i].get(),i,1);
    }
    timerArea = new QScrollArea(this);
    timerArea->setWidgetResizable(true);
    timerArea->setWidget(timerGridWidget);

    // Create the memory plot
    if ( !parent->d_data.memory.empty() )
        memory = new MemoryPlot( this, parent->d_data.memory );
    else
        memory = new QWidget( );

    // Create the layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setContentsMargins(QMargins(0,0,0,0));
    layout->setSpacing(0);
    layout->addWidget(timeline);
    layout->addWidget(timerArea);
    layout->addWidget(memory);
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);

    // Create the toolbars
    createToolBars();

    // Create resize event
    resizeTimer.setSingleShot( true );
    connect( &resizeTimer, SIGNAL(timeout()), SLOT(resizeDone()) );

    // Plot the data
    timelineBoundaries[0].reset(new CurrentTimeLineClass(timeline,this,t_global,0,t_global[0]));
    timelineBoundaries[1].reset(new CurrentTimeLineClass(timeline,this,t_global,1,t_global[1]));
    zoomBoundaries[0].reset( new DrawVerticalLineClass(timerArea,0.1,0) );
    zoomBoundaries[1].reset( new DrawVerticalLineClass(timerArea,0.9,0) );
    zoomBoundaries[0]->setVisible(false);
    zoomBoundaries[1]->setVisible(false);
    reset();

    PROFILE_STOP("TraceWindow");
}
TraceWindow::~TraceWindow()
{
}
void TraceWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}


/***********************************************************************
* Reset all internal data                                              *
***********************************************************************/
void TraceWindow::reset()
{
    PROFILE_START("reset");
    // Reset t_min and t_max
    t_current = t_global;
    // Update the data
    updateDisplay( UpdateType::all );
    PROFILE_STOP("reset");
}


/***********************************************************************
* Update the display                                                   *
***********************************************************************/
void TraceWindow::updateDisplay( UpdateType update )
{
    PROFILE_START("updateDisplay");
    // Regenerate the global timeline if the processors changed
    if ( (update&UpdateType::time)!=0 )
        updateTimeline();
    // Update the line positions if time changed

    // Regerate the trace plots if time or processors changed
    if ( (update&UpdateType::time)!=0 || (update&UpdateType::proc)!=0 )
        updateTimers();
    // Regerate the memory plot if time or processors changed
    if ( (update&UpdateType::time)!=0 || (update&UpdateType::proc)!=0 )
        updateMemory();
    // Run resize to update images
    resizeDone();
    PROFILE_STOP("updateDisplay");
}


/***********************************************************************
* Get the trace data                                                   *
***********************************************************************/
std::vector<std::shared_ptr<TimerTimeline>> TraceWindow::getTraceData( const std::array<double,2>& t ) const
{
    PROFILE_START("getTraceData");
    // Get the active timers/traces
    //std::vector<std::shared_ptr<TimerSummary>> timers = parent->getTimers();
    const std::vector<TimerResults>& timers = parent->d_data.timers;
    // Create a timeline for the time of interest
    std::vector<std::shared_ptr<TimerTimeline>> data(timers.size());
    int Np = selected_rank==-1 ? N_procs:1;
    int Nt = selected_thread==-1 ? N_threads:1;
    for (size_t i=0; i<timers.size(); i++) {
        data[i].reset(new TimerTimeline);
        data[i]->id = timers[i].id;
        data[i]->message = timers[i].message;
        data[i]->file = timers[i].file;
        BoolArray& array = data[i]->active;
        array.resize(resolution,Nt,Np);
        data[i]->tot = 0;
        for (size_t j=0; j<timers[i].trace.size(); j++) {
            const int rank = timers[i].trace[j].rank;
            const int thread = timers[i].trace[j].thread;
            const int N_trace = timers[i].trace[j].N_trace;
            const double* start = timers[i].trace[j].start();
            const double* stop  = timers[i].trace[j].stop();
            int it = Nt==1 ? 0:thread;
            int ip = Np==1 ? 0:rank;
            if ( selected_thread!=-1 && thread!=selected_thread )
                continue;
            if ( selected_rank!=-1 && rank!=selected_rank )
                continue;
            for (int k=0; k<N_trace; k++) {
                if ( stop[k]<=t[0] || start[k]>=t[1] )
                    continue;
                data[i]->tot += std::min(stop[k],t[1])-std::max(start[k],t[0]);
                for (int k2=0; k2<resolution; k2++) {
                    double time = t[0] + k2*(t[1]-t[0])/(resolution-1);
                    if ( time>=start[k] && time<=stop[k] )
                        array.set(k2,it,ip);
                }
            }
        }
    }
    // Sort the data by the total time spent in each routine
    std::multimap<double,std::shared_ptr<TimerTimeline>> tmp;
    for (size_t i=0; i<timers.size(); i++)
        tmp.insert(std::pair<double,std::shared_ptr<TimerTimeline>>(data[i]->tot,data[i]));
    data.clear();
    for (auto it : tmp) {
        if ( it.second->tot > 0 )
            data.push_back(it.second);
    }
    PROFILE_STOP("getTraceData");
    return data;
}


/***********************************************************************
* Update the timeline plot                                             *
***********************************************************************/
void TraceWindow::updateTimeline()
{
    PROFILE_START("updateTimeline");
    const uint64_t rgb0 = jet(-1);
    // Get the data for the entire window
    std::vector<std::shared_ptr<TimerTimeline>> data = TraceWindow::getTraceData(t_global);
    // Create the global id map
    for (size_t i=0; i<parent->d_data.timers.size(); i++)
        idRgbMap[parent->d_data.timers[i].id] = rgb0;
    for (size_t i=0; i<data.size(); i++)
        idRgbMap[data[i]->id] = jet(i/(data.size()-1.0));
    // Create the image
    QImage *image = NULL;
    int Np = selected_rank==-1 ? N_procs:1;
    int Nt = selected_thread==-1 ? N_threads:1;
    if ( Np*Nt*data.size() <= 256 ) {
        image = new QImage(resolution,Np*Nt*data.size(),QImage::Format_RGB32);
        for (size_t i=0; i<data.size(); i++) {
            uint64_t rgb = idRgbMap[data[i]->id];
            const BoolArray& active = data[i]->active;
            for (int k=0; k<Np*Nt; k++) {
                for (int j=0; j<resolution; j++) {
                    uint64_t value = active(j,k) ? rgb:rgb0;
                    image->setPixel(j,k+i*Np*Nt,value);
                }
            }
        }
    } else if ( Np*data.size() <= 256 ) {
        image = new QImage(resolution,N_procs*data.size(),QImage::Format_RGB32);
        for (size_t i=0; i<data.size(); i++) {
            uint64_t rgb = idRgbMap[data[i]->id];
            const BoolArray& active = data[i]->active;
            for (int k=0; k<Np; k++) {
                for (int j=0; j<resolution; j++) {
                    bool set = false;
                    for (int k2=0; k2<Nt; k2++)
                        set = set || active(j,k2,k);
                    uint64_t value = set ? rgb:rgb0;
                    image->setPixel(j,k+i*N_procs,value);
                }
            }
        }
    } else {
        image = new QImage(resolution,data.size(),QImage::Format_RGB32);
        for (size_t i=0; i<data.size(); i++) {
            uint64_t rgb = idRgbMap[data[i]->id];
            const BoolArray& active = data[i]->active;
            for (int j=0; j<resolution; j++) {
                bool set = false;
                for (int k=0; k<Np*Nt; k++)
                    set = set || active(j,k);
                uint64_t value = set ? rgb:rgb0;
                image->setPixel(j,i,value);
            }
        }
    }
    timelinePixelMap.reset( new QPixmap(QPixmap::fromImage(*image)) );
    delete image;
    PROFILE_STOP("updateTimeline");
}


/***********************************************************************
* Update the timer plots                                               *
***********************************************************************/
void TraceWindow::updateTimers()
{
    PROFILE_START("updateTimers");
    // Get the data
    std::vector<std::shared_ptr<TimerTimeline>> data = TraceWindow::getTraceData(t_current);
    // Clear the existing data from the table
    for (size_t i=0; i<timerLabels.size(); i++) {
        timerLabels[i]->clear();
        timerPlots[i]->clear();
        timerGrid->removeWidget(timerLabels[i].get());
        timerGrid->removeWidget(timerPlots[i].get());
    }
    timerPixelMap.clear();
    // Add the labels and plots
    const uint64_t rgb0 = jet(-1);
    timerPixelMap.resize(data.size());
    int Np = selected_rank==-1 ? N_procs:1;
    int Nt = selected_thread==-1 ? N_threads:1;
    for (size_t i=0; i<data.size(); i++) {
        timerLabels[i]->setText(data[i]->message.c_str());
        QImage image(resolution,Np*Nt,QImage::Format_RGB32);
        uint64_t rgb = idRgbMap[data[i]->id];
        const BoolArray& active = data[i]->active;
        for (int j=0; j<resolution; j++) {
            for (int k=0; k<Np*Nt; k++) {
                uint64_t value = active(j,k) ? rgb:rgb0;
                image.setPixel(j,k,value);
            }
        }
        timerPixelMap[i].reset( new QPixmap(QPixmap::fromImage(image)) );
        timerPlots[i]->setPixmap(*timerPixelMap[i]);
        timerGrid->addWidget(timerLabels[i].get(),i,0);
        timerGrid->addWidget(timerPlots[i].get(),i,1);
    }
    PROFILE_STOP("updateTimers");
}

/***********************************************************************
* Update the memory plot                                               *
***********************************************************************/
void TraceWindow::updateMemory()
{
    if ( parent->d_data.memory.empty() ) {
        memory->setVisible(false);
        return;
    }
    PROFILE_START("updateMemory");
    dynamic_cast<MemoryPlot*>(memory)->plot( t_current, selected_rank );
    PROFILE_STOP("updateMemory");
}


/***********************************************************************
* Resize functions                                                     *
***********************************************************************/
void TraceWindow::resizeEvent( QResizeEvent *e )
{
    resizeTimer.start( 100 );
    QMainWindow::resizeEvent(e);
}
void TraceWindow::resizeDone()
{
    PROFILE_START("resizeDone");
    // Scale the timeline window
    int w = timeline->rect().width();
    int h = timeline->rect().height();
    timeline->setFixedHeight(centralWidget()->height()/8);
    timeline->setPixmap(timelinePixelMap->scaled(w,h,Qt::IgnoreAspectRatio,Qt::FastTransformation));
    timelineBoundaries[0]->setTime(t_current[0]);
    timelineBoundaries[1]->setTime(t_current[1]);
    // Scale the timers
    for (size_t i=0; i<timerPixelMap.size(); i++) {
        int w2 = timerPlots[i]->width();
        int h2 = timerPlots[i]->height();
        timerPlots[i]->setPixmap(timerPixelMap[i]->scaled(w2,h2,Qt::IgnoreAspectRatio,Qt::FastTransformation));        
    }
    if ( zoomBoundaries[0].get() != NULL ) {
        zoomBoundaries[0]->redraw();
        zoomBoundaries[1]->redraw();
    }
    // Resize the memory axis to align with the timers
    MemoryPlot* memplot = dynamic_cast<MemoryPlot*>(memory);
    if ( memplot != NULL ) {
        int left = timerPlots[0]->x();
        int right = left + timerPlots[0]->width();
        memplot->align(left,right);
    }
    PROFILE_STOP("resizeDone");
}


/***********************************************************************
* Select a processor/thread                                            *
***********************************************************************/
void TraceWindow::selectProcessor( int rank )
{
    selected_rank = rank;
    if ( rank==-1 ) {
        processorButton->setText("Processor: All");
    } else {
        processorButton->setText(stringf("Rank %i",rank).c_str());
    }
    if ( N_procs == 1 ) {
        selected_rank = -1;
        return;
    }
    updateDisplay(UpdateType::proc);
}
void TraceWindow::selectThread( int thread )
{
    selected_thread = thread;
    if ( thread==-1 ) {
        threadButton->setText("Thread: All");
    } else {
        threadButton->setText(stringf("Thread %i",thread).c_str());
    }
    if ( N_threads == 1 ) {
        selected_thread = -1;
        return;
    }
    updateDisplay(UpdateType::proc);
}


/***********************************************************************
* Change the resolution                                                *
***********************************************************************/
void TraceWindow::resolutionChanged()
{
    int resolution_old = resolution;
    resolutionBox->clearFocus();
    int res = static_cast<int>(resolutionBox->text().toDouble());
    if ( res > 0 ) {
        res = std::max(res,10);
        res = std::min(res,10000);
        resolution = res;
    }
    resolutionBox->setText(stringf("%i",resolution).c_str());
    if ( resolution == resolution_old )
        return;
    updateDisplay(UpdateType::all);
}


/***********************************************************************
* Change the time                                                      *
***********************************************************************/
void TraceWindow::moveTimelineWindow( int index, double t )
{
    std::array<double,2> t_new = t_current;
    t_new[index] = t;
    if ( std::max<double>(t_new[1]-t_new[0],0) < 0.1*(t_current[1]-t_current[0]) ) {
        if ( index==0 )
            t_new[0] = t_current[1] - 0.1*(t_current[1]-t_current[0]);
        else
            t_new[1] = t_current[0] + 0.1*(t_current[1]-t_current[0]);
    }
    t_current = t_new;
    updateDisplay(UpdateType::time);
}


/***********************************************************************
* Create the toolbar                                                   *
***********************************************************************/
void TraceWindow::createToolBars()
{
    QToolBar *toolBar = addToolBar(tr("Trace"));

    toolBar->addSeparator();
    QAction *resetAct = new QAction(QIcon(":/images/refresh.png"), tr("&Reset"), this);
    resetAct->setShortcuts(QKeySequence::Refresh);
    resetAct->setStatusTip(tr("Reset the view"));
    connect(resetAct, SIGNAL(triggered()), this, SLOT(reset()));
    toolBar->addAction(resetAct);

    // Processor popup
    toolBar->addSeparator();
    processorButton = new QToolButton();
    processorButton->setPopupMode(QToolButton::InstantPopup);
    toolBar->addWidget(processorButton);
    QMenu *menu = new QMenu();
    QSignalMapper* signalMapper = new QSignalMapper(this);
    ADD_MENU_ACTION(menu,"All",-1);
    for (int i=0; i<N_procs; i++)
        ADD_MENU_ACTION(menu,stringf("Rank %i",i).c_str(),i);
    connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(selectProcessor(int)));
    processorButton->setText("Processor: All");
    processorButton->setMenu(menu);
    toolBar->addSeparator();

    // Thread popup
    toolBar->addSeparator();
    threadButton = new QToolButton();
    threadButton->setPopupMode(QToolButton::InstantPopup);
    toolBar->addWidget(threadButton);
    menu = new QMenu();
    signalMapper = new QSignalMapper(this);
    ADD_MENU_ACTION(menu,"All",-1);
    for (int i=0; i<N_threads; i++)
        ADD_MENU_ACTION(menu,stringf("Thread %i",i).c_str(),i);
    connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(selectThread(int)));
    threadButton->setText("Thread: All");
    threadButton->setMenu(menu);
    toolBar->addSeparator();

    // Resolution
    QAction *tmp = toolBar->addAction("Resolution:");
    tmp->setCheckable(false);
    //tmp->setEnabled(false);
    resolutionBox = new QLineEdit(stringf("%i",resolution).c_str());
    resolutionBox->setMaxLength(6);
    resolutionBox->setMaximumWidth(60);
    toolBar->addWidget(resolutionBox);
    connect(resolutionBox, SIGNAL(editingFinished()), this, SLOT(resolutionChanged()));

}


/***********************************************************************
* Helper function to return the global start/stop time                 *
***********************************************************************/
std::array<double,2> TraceWindow::getGlobalTime( const std::vector<TimerResults>& timers )
{
    std::array<double,2> t_global;
    t_global[0] = 1e100;
    t_global[1] = -1e100;
    for (size_t i=0; i<timers.size(); i++) {
        for (size_t j=0; j<timers[i].trace.size(); j++) {
            const int N_trace = timers[i].trace[j].N_trace;
            const double* start = timers[i].trace[j].start();
            const double* stop  = timers[i].trace[j].stop();
            t_global[0] = std::min(t_global[0],start[0]);
            t_global[1] = std::max(t_global[1],stop[N_trace-1]);
        }
    }
    return t_global;
}


/***********************************************************************
* Create/move the trace sliders for the detailed timeline              *
***********************************************************************/
void TraceWindow::traceMousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        traceZoomActive = true;
        traceZoomLastPos = event->globalPos().x();
        int x = timerArea->mapFromGlobal(event->globalPos()).x();
        double pos = ((double)x-timerArea->rect().left())/timerArea->rect().width();
        zoomBoundaries[0]->move(pos);
        zoomBoundaries[1]->move(pos);
        zoomBoundaries[0]->setVisible(true);
        zoomBoundaries[1]->setVisible(true);
        int x2 = timerPlots[0]->mapFromGlobal(event->globalPos()).x();
        double t = t_current[0] + x2*(t_current[1]-t_current[0])/timerPlots[0]->rect().width();
        t_zoom[0] = t;
        t_zoom[1] = t;
    }
}
void TraceWindow::traceMouseMoveEvent(QMouseEvent *event)
{
    int x0 = event->globalPos().x();
    if ( !traceZoomActive && abs(x0-traceZoomLastPos)<4 )
        return;
    traceZoomLastPos = x0;
    int x = timerArea->mapFromGlobal(event->globalPos()).x();
    double pos = ((double)x-timerArea->rect().left())/timerArea->rect().width();
    zoomBoundaries[1]->move(pos);
}
void TraceWindow::traceMouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        traceZoomActive = false;
        int x2 = timerPlots[0]->mapFromGlobal(event->globalPos()).x();
        double t = t_current[0] + x2*(t_current[1]-t_current[0])/timerPlots[0]->rect().width();
        t_zoom[1] = t;
        if ( t_zoom[1] < t_zoom[0] )
            std::swap(t_zoom[0],t_zoom[1]);
        t_zoom[0] = std::max(t_zoom[0],t_global[0]);
        t_zoom[1] = std::min(t_zoom[1],t_global[1]);
        zoomBoundaries[0]->setVisible(false);
        zoomBoundaries[1]->setVisible(false);
        if ( t_zoom[1]-t_zoom[0] > 0.02*(t_current[1]-t_current[0]) ) {
            t_current = t_zoom;
            updateDisplay(UpdateType::time);
        }
    }
}


