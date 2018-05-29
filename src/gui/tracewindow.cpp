#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QStatusBar>
#include <QTextEdit>
#include <QtGui>

#include <array>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "MemoryPlot.h"
#include "TableValue.h"
#include "colormap.h"
#include "tracewindow.h"


// Class to draw a vertical line
class DrawVerticalLineClass : public QWidget
{
public:
    DrawVerticalLineClass( QWidget *parent, double pos_, int boarder = 0 )
        : QWidget( parent ), d_parent( parent ), d_boarder( boarder ), pos( pos_ )
    {
        move( pos );
    }
    // Move to the given position
    void move( double pos_ )
    {
        pos = pos_;
        pos = std::max( pos, 0.0 );
        pos = std::min( pos, 1.0 );
        int x, y, w, h;
        d_parent->rect().getRect( &x, &y, &w, &h );
        setGeometry( QRect( x + w * pos - d_boarder, y, 2 + 2 * d_boarder, h ) );
        this->update();
    }
    // Update
    void redraw() { move( pos ); }
    // Draw the rectangle
    void paintEvent( QPaintEvent * ) override
    {
        QPainter p( this );
        p.setRenderHint( QPainter::Antialiasing );
        int x, y, w, h;
        rect().getRect( &x, &y, &w, &h );
        p.fillRect( QRect( x + d_boarder, y, 2, h ), QBrush( QColor( 255, 255, 255 ) ) );
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
    CurrentTimeLineClass( QWidget *parent, TraceWindow *traceWindow,
        const std::array<double, 2> &t_global, int id, double t0 )
        : DrawVerticalLineClass( parent, 0, 3 ),
          d_traceWindow( traceWindow ),
          d_t_global( t_global ),
          d_t( t0 ),
          pos0( 0 ),
          d_id( id ),
          start( 0 ),
          last( 0 ),
          active( false )
    {
        setMouseTracking( true );
        setTime( t0 );
    }
    void setTime( double t )
    {
        d_t  = t;
        pos0 = ( t - d_t_global[0] ) / ( d_t_global[1] - d_t_global[0] );
        move( pos0 );
    }
    void mousePressEvent( QMouseEvent *event ) override
    {
        if ( event->button() == Qt::LeftButton ) {
            start  = event->globalPos().x();
            last   = start;
            active = true;
        }
    }
    void mouseMoveEvent( QMouseEvent *event ) override
    {
        int pos = event->globalPos().x();
        if ( !active || abs( last - pos ) < 4 )
            return;
        last        = pos;
        double w    = d_parent->rect().width();
        double pos2 = pos0 + static_cast<double>( pos - start ) / w;
        move( pos2 );
    }
    void mouseReleaseEvent( QMouseEvent *event ) override
    {
        if ( event->button() == Qt::LeftButton ) {
            int pos     = event->globalPos().x();
            double w    = d_parent->rect().width();
            double pos2 = pos0 + static_cast<double>( pos - start ) / w;
            double t    = d_t_global[0] + pos2 * ( d_t_global[1] - d_t_global[0] );
            t           = std::max( t, d_t_global[0] );
            t           = std::min( t, d_t_global[1] );
            active      = false;
            start = last = 0;
            setTime( t );
            d_traceWindow->moveTimelineWindow( d_id, t );
        }
    }

private:
    TraceWindow *d_traceWindow;
    const std::array<double, 2> d_t_global;
    double d_t, pos0;
    int d_id;
    int start, last;
    bool active;
};


// Class for QLabel that has mouse movement functions
class QLabelMouse : public QLabel
{
public:
    explicit QLabelMouse( TraceWindow *trace ) : QLabel( nullptr ), d_trace( trace ) {}
    void mousePressEvent( QMouseEvent *event ) override { d_trace->traceMousePressEvent( event ); }
    void mouseMoveEvent( QMouseEvent *event ) override { d_trace->traceMouseMoveEvent( event ); }
    void mouseReleaseEvent( QMouseEvent *event ) override
    {
        d_trace->traceMouseReleaseEvent( event );
    }

private:
    TraceWindow *d_trace;
};


// Compute the sum of a vector
template<class TYPE>
TYPE sum( const std::vector<TYPE> &x )
{
    TYPE y = 0;
    for ( size_t i = 0; i < x.size(); i++ )
        y += x[i];
    return y;
}


// Function to add an action (with connection) to a menu
#define ADD_MENU_ACTION( menu, string, arg )                                   \
    do {                                                                       \
        QAction *action = new QAction( string, this );                         \
        connect( action, SIGNAL( triggered() ), signalMapper, SLOT( map() ) ); \
        signalMapper->setMapping( action, arg );                               \
        menu->addAction( action );                                             \
    } while ( 0 )


/***********************************************************************
 * Constructor/destructor                                               *
 ***********************************************************************/
TraceWindow::TraceWindow( const TimerWindow *parent_ )
    : ThreadedSlotsClass(),
      timerGrid( nullptr ),
      memory( nullptr ),
      parent( parent_ ),
      N_procs( parent_->N_procs ),
      N_threads( parent_->N_threads ),
      t_global( getGlobalTime( parent_->d_data.timers ) ),
      resolution( 1024 ),
      selected_rank( -1 ),
      selected_thread( -1 )
{
    qApp->processEvents();
    PROFILE_START( "TraceWindow" );
    QWidget::setWindowTitle( QString( "Trace results: " ).append( parent->windowTitle() ) );
    t_current = t_global;
    resize( 1200, 800 );

    // Create the master timeline
    timeline = new QLabel;
    timeline->setScaledContents( true );
    timeline->setMinimumSize( 10, 10 );

    // Create the timer timelines
    int N_rows = parent->d_data.timers.size();
    timerGrid  = new QSplitterGrid();
    timerGrid->tableSize( N_rows, 2 );
    timerGrid->setSpacing( 0 );
    timerGrid->setVerticalSpacing( 2 );
    timerGrid->setHorizontalSpacing( 5 );
    timerGrid->setUniformRowHeight( true );
    timerGrid->setUniformColumnWidth( false );
    timerLabels.resize( N_rows, nullptr );
    timerPlots.resize( N_rows, nullptr );
    for ( int i = 0; i < N_rows; i++ ) {
        timerLabels[i] = new QLabel();
        timerLabels[i]->setMinimumWidth( 30 );
        timerLabels[i]->setText( "" );
        timerGrid->addWidget( timerLabels[i], i, 0 );
        timerPlots[i] = new QLabelMouse( this );
        timerPlots[i]->setScaledContents( true );
        timerPlots[i]->setMinimumSize( 200, 20 );
        timerGrid->addWidget( timerPlots[i], i, 1 );
    }
    timerGrid->setRowHeight( 35 );

    // Create the memory plot
    memory = new MemoryPlot( this, parent->d_data.memory );
    auto f = std::bind( &TraceWindow::resizeMemory, this );
    timerGrid->registerResizeCallback( f );

    // Create the layout
    auto *layout = new QVBoxLayout;
    layout->setMargin( 0 );
    layout->setContentsMargins( QMargins( 0, 0, 0, 0 ) );
    layout->setSpacing( 0 );
    layout->addWidget( timeline );
    layout->addWidget( timerGrid );
    layout->addWidget( memory );
    setCentralWidget( new QWidget );
    centralWidget()->setLayout( layout );

    // Create the toolbars
    createToolBars();

    // Create resize event
    resizeTimer.setSingleShot( true );
    connect( &resizeTimer, SIGNAL( timeout() ), SLOT( resizeDone() ) );

    // Plot the data
    timelineBoundaries[0].reset(
        new CurrentTimeLineClass( timeline, this, t_global, 0, t_global[0] ) );
    timelineBoundaries[1].reset(
        new CurrentTimeLineClass( timeline, this, t_global, 1, t_global[1] ) );
    zoomBoundaries[0].reset( new DrawVerticalLineClass( timerGrid, 0.1, 0 ) );
    zoomBoundaries[1].reset( new DrawVerticalLineClass( timerGrid, 0.9, 0 ) );
    zoomBoundaries[0]->setVisible( false );
    zoomBoundaries[1]->setVisible( false );
    reset();

    qApp->processEvents();
    PROFILE_STOP( "TraceWindow" );
}
TraceWindow::~TraceWindow()
{
    qApp->processEvents();
    delete threadButtonMenu;
    delete processorButtonMenu;
}
void TraceWindow::closeEvent( QCloseEvent *event )
{
    event->accept();
    parent->traceWindow.reset(); // Force the deletion of this
}


/***********************************************************************
 * Reset all internal data                                              *
 ***********************************************************************/
void TraceWindow::reset()
{
    PROFILE_START( "reset" );
    // Reset t_min and t_max
    t_current = t_global;
    // Reset the column widths
    timerGrid->setColumnWidth( { 60, 400 } );
    // Update the data
    updateDisplay( UpdateType::all );
    PROFILE_STOP( "reset" );
}
void TraceWindow::resetZoom()
{
    PROFILE_START( "resetZoom" );
    // Reset t_min and t_max
    t_current = t_global;
    // Update the data
    updateDisplay( UpdateType::all );
    PROFILE_STOP( "resetZoom" );
}


/***********************************************************************
 * Update the display                                                   *
 ***********************************************************************/
void TraceWindow::updateDisplay( UpdateType update )
{
    PROFILE_START( "updateDisplay" );
    // Regenerate the global timeline if the processors changed
    if ( ( update & UpdateType::header ) != 0 )
        updateTimeline();
    // Update the line positions if time changed

    // Regerate the trace plots if time or processors changed
    if ( ( update & UpdateType::time ) != 0 || ( update & UpdateType::proc ) != 0 )
        updateTimers();
    // Regerate the memory plot if time or processors changed
    if ( ( update & UpdateType::time ) != 0 || ( update & UpdateType::proc ) != 0 )
        updateMemory();
    // Start the resize timer to update plots
    resizeTimer.start( 10 );
    PROFILE_STOP( "updateDisplay" );
}


/***********************************************************************
 * Get the trace data                                                   *
 ***********************************************************************/
std::vector<std::shared_ptr<TimerTimeline>> TraceWindow::getTraceData(
    const std::array<double, 2> &t ) const
{
    PROFILE_START( "getTraceData" );
    // Get the active timers/traces
    const auto &timers = parent->d_data.timers;
    // Create a timeline for the time of interest
    std::vector<std::shared_ptr<TimerTimeline>> data( timers.size() );
    int Np          = selected_rank == -1 ? N_procs : 1;
    int Nt          = selected_thread == -1 ? N_threads : 1;
    const double t0 = t[0];
    const double t1 = t[1];
    const double dt = ( t[1] - t[0] ) / ( resolution - 1 );
    for ( size_t i = 0; i < timers.size(); i++ ) {
        data[i].reset( new TimerTimeline );
        data[i]->id      = timers[i].id;
        data[i]->message = timers[i].message;
        data[i]->file    = timers[i].file;
        BoolArray &array = data[i]->active;
        array.resize( resolution, Nt, Np );
        data[i]->tot = 0;
        for ( size_t j = 0; j < timers[i].trace.size(); j++ ) {
            const int rank    = timers[i].trace[j].rank;
            const int thread  = timers[i].trace[j].thread;
            const int N_trace = timers[i].trace[j].N_trace;
            const auto start  = timers[i].trace[j].start();
            const auto stop   = timers[i].trace[j].stop();
            int it            = Nt == 1 ? 0 : thread;
            int ip            = Np == 1 ? 0 : rank;
            if ( selected_thread != -1 && thread != selected_thread )
                continue;
            if ( selected_rank != -1 && rank != selected_rank )
                continue;
            for ( int k = 0; k < N_trace; k++ ) {
                double s1 = 1e-9 * start[k];
                double s2 = 1e-9 * stop[k];
                if ( s2 <= t0 || s1 >= t1 )
                    continue;
                int m1 = std::max<int>( ( s1 - t0 ) / dt, 0 );
                int m2 = std::min<int>( ( s2 - t0 ) / dt, resolution - 1 );
                for ( int k2 = m1; k2 <= m2; k2++ )
                    array.set( k2, it, ip );
                data[i]->tot += std::min( s2, t1 ) - std::max( s1, t0 );
            }
        }
    }
    // Sort the data by the total time spent in each routine
    std::multimap<double, std::shared_ptr<TimerTimeline>> tmp;
    for ( size_t i = 0; i < timers.size(); i++ )
        tmp.insert( std::pair<double, std::shared_ptr<TimerTimeline>>( data[i]->tot, data[i] ) );
    data.clear();
    for ( auto it : tmp ) {
        if ( it.second->tot > 0 )
            data.push_back( it.second );
    }
    PROFILE_STOP( "getTraceData" );
    return data;
}


/***********************************************************************
 * Update the timeline plot                                             *
 ***********************************************************************/
void TraceWindow::updateTimeline()
{
    PROFILE_START( "updateTimeline" );
    const auto rgb0 = jet( -1 );
    // Get the data for the entire window
    auto data = TraceWindow::getTraceData( t_global );
    // Create the global id map
    for ( const auto &timer : parent->d_data.timers )
        idRgbMap[timer.id] = rgb0;
    for ( size_t i = 0; i < data.size(); i++ )
        idRgbMap[data[i]->id] = jet( i / ( data.size() - 1.0 ) );
    // Create the image
    QImage *image = nullptr;
    int Np        = selected_rank == -1 ? N_procs : 1;
    int Nt        = selected_thread == -1 ? N_threads : 1;
    if ( Np * Nt * data.size() <= 256 ) {
        image = new QImage( resolution, Np * Nt * data.size(), QImage::Format_RGB32 );
        for ( size_t i = 0; i < data.size(); i++ ) {
            auto rgb     = idRgbMap[data[i]->id];
            auto &active = data[i]->active;
            for ( int k = 0; k < Np * Nt; k++ ) {
                for ( int j = 0; j < resolution; j++ ) {
                    uint32_t value = active( j, k ) ? rgb : rgb0;
                    image->setPixel( j, k + i * Np * Nt, value );
                }
            }
        }
    } else if ( Np * data.size() <= 256 ) {
        image = new QImage( resolution, N_procs * data.size(), QImage::Format_RGB32 );
        for ( size_t i = 0; i < data.size(); i++ ) {
            auto rgb     = idRgbMap[data[i]->id];
            auto &active = data[i]->active;
            for ( int k = 0; k < Np; k++ ) {
                for ( int j = 0; j < resolution; j++ ) {
                    bool set = false;
                    for ( int k2 = 0; k2 < Nt; k2++ )
                        set = set || active( j, k2, k );
                    uint32_t value = set ? rgb : rgb0;
                    image->setPixel( j, k + i * N_procs, value );
                }
            }
        }
    } else {
        image = new QImage( resolution, data.size(), QImage::Format_RGB32 );
        for ( size_t i = 0; i < data.size(); i++ ) {
            auto rgb     = idRgbMap[data[i]->id];
            auto &active = data[i]->active;
            for ( int j = 0; j < resolution; j++ ) {
                bool set = false;
                for ( int k = 0; k < Np * Nt; k++ )
                    set = set || active( j, k );
                uint32_t value = set ? rgb : rgb0;
                image->setPixel( j, i, value );
            }
        }
    }
    if ( data.size() >= 512 ) {
        int Ns      = data.size() / 256;
        int N       = data.size() / Ns;
        int N0      = data.size();
        auto image0 = image;
        image       = new QImage( resolution, N, QImage::Format_RGB32 );
        for ( int j = 0; j < resolution; j++ ) {
            for ( int i = 0; i < N; i++ ) {
                auto val = rgb0;
                int n    = 0;
                for ( int k = 0; k < Ns && i * Ns + k < N0 && val == rgb0; k++, n++ )
                    val = image0->pixel( j, i * Ns + k );
                image->setPixel( j, i, val );
            }
        }
        delete image0;
    }
    timelinePixelMap.reset( new QPixmap( QPixmap::fromImage( *image ) ) );
    delete image;
    PROFILE_STOP( "updateTimeline" );
}


/***********************************************************************
 * Update the timer plots                                               *
 ***********************************************************************/
void TraceWindow::updateTimers()
{
    PROFILE_START( "updateTimers" );
    // Get the data
    auto data = TraceWindow::getTraceData( t_current );
    std::vector<bool> rowVisible( timerGrid->numberRows(), false );
    auto rgb0 = jet( -1 );
    int Np    = selected_rank == -1 ? N_procs : 1;
    int Nt    = selected_thread == -1 ? N_threads : 1;
    for ( size_t i = 0; i < data.size(); i++ ) {
        rowVisible[i] = true;
        char label[1000];
        sprintf( label,
            "<font size=\"3\" color=\"black\">%s</font><br>"
            "<font size=\"2\" color=\"gray\">%s</font>",
            data[i]->message.c_str(), data[i]->file.c_str() );
        timerLabels[i]->setText( label );
        QImage image( resolution, Np * Nt, QImage::Format_RGB32 );
        auto rgb     = idRgbMap[data[i]->id];
        auto &active = data[i]->active;
        for ( int j = 0; j < resolution; j++ ) {
            for ( int k = 0; k < Np * Nt; k++ ) {
                uint32_t value = active( j, k ) ? rgb : rgb0;
                image.setPixel( j, k, value );
            }
        }
        timerPlots[i]->setPixmap( QPixmap::fromImage( image ) );
    }
    timerGrid->setRowVisible( rowVisible );
    PROFILE_STOP( "updateTimers" );
}


/***********************************************************************
 * Update the memory plot                                               *
 ***********************************************************************/
void TraceWindow::updateMemory()
{
    memory->setVisible( true );
    PROFILE_START( "updateMemory" );
    dynamic_cast<MemoryPlot *>( memory )->plot( t_current, selected_rank );
    PROFILE_STOP( "updateMemory" );
}


/***********************************************************************
 * Resize functions                                                     *
 ***********************************************************************/
void TraceWindow::resizeEvent( QResizeEvent *e )
{
    resizeTimer.start( 100 );
    QMainWindow::resizeEvent( e );
}
void TraceWindow::resizeDone()
{
    PROFILE_START( "resizeDone" );
    // Scale the timeline window
    int w = timeline->rect().width();
    int h = timeline->rect().height();
    timeline->setFixedHeight( centralWidget()->height() / 8 );
    timeline->setPixmap(
        timelinePixelMap->scaled( w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation ) );
    timelineBoundaries[0]->setTime( t_current[0] );
    timelineBoundaries[1]->setTime( t_current[1] );
    // Scale the timers
    if ( zoomBoundaries[0].get() != nullptr ) {
        zoomBoundaries[0]->redraw();
        zoomBoundaries[1]->redraw();
    }
    // Resize the grid (will call resize on the memory plot automatically)
    auto cw = timerGrid->getColumnWidth();
    int w2  = timerGrid->geometry().width() - 2 * timerGrid->getHorizontalSpacing() - 30;
    timerGrid->setColumnWidth(
        { ( cw[0] * w2 ) / ( cw[0] + cw[1] ), ( cw[1] * w2 ) / ( cw[0] + cw[1] ) } );
    PROFILE_STOP( "resizeDone" );
}
void TraceWindow::resizeMemory()
{
    PROFILE_START( "resizeMemory" );
    auto *memplot = dynamic_cast<MemoryPlot *>( memory );
    QRect pos     = timerGrid->getPosition( 0, 1 );
    int left      = pos.x();
    int right     = left + pos.width();
    memplot->align( left, right );
    if ( parent->d_data.memory.empty() )
        memplot->setFixedHeight( 45 );
    else
        memplot->setFixedHeight( centralWidget()->height() / 4 );
    PROFILE_STOP( "resizeMemory" );
}


/***********************************************************************
 * Select a processor/thread                                            *
 ***********************************************************************/
void TraceWindow::selectProcessor( int rank )
{
    selected_rank = rank;
    if ( rank == -1 ) {
        processorButton->setText( "Processor: All" );
    } else {
        processorButton->setText( stringf( "Rank %i", rank ).c_str() );
    }
    if ( N_procs == 1 ) {
        selected_rank = -1;
        return;
    }
    updateDisplay( UpdateType::proc );
}
void TraceWindow::selectThread( int thread )
{
    selected_thread = thread;
    if ( thread == -1 ) {
        threadButton->setText( "Thread: All" );
    } else {
        threadButton->setText( stringf( "Thread %i", thread ).c_str() );
    }
    if ( N_threads == 1 ) {
        selected_thread = -1;
        return;
    }
    updateDisplay( UpdateType::proc );
}


/***********************************************************************
 * Change the resolution                                                *
 ***********************************************************************/
void TraceWindow::resolutionChanged()
{
    int resolution_old = resolution;
    resolutionBox->clearFocus();
    int res = static_cast<int>( resolutionBox->text().toDouble() );
    if ( res > 0 ) {
        res        = std::max( res, 10 );
        res        = std::min( res, 10000 );
        resolution = res;
    }
    resolutionBox->setText( stringf( "%i", resolution ).c_str() );
    if ( resolution == resolution_old )
        return;
    updateDisplay( UpdateType::all );
}


/***********************************************************************
 * Change the time                                                      *
 ***********************************************************************/
void TraceWindow::moveTimelineWindow( int index, double t )
{
    auto t_new   = t_current;
    t_new[index] = t;
    if ( std::max<double>( t_new[1] - t_new[0], 0 ) < 0.1 * ( t_current[1] - t_current[0] ) ) {
        if ( index == 0 )
            t_new[0] = t_current[1] - 0.1 * ( t_current[1] - t_current[0] );
        else
            t_new[1] = t_current[0] + 0.1 * ( t_current[1] - t_current[0] );
    }
    t_current = t_new;
    updateDisplay( UpdateType::time );
}


/***********************************************************************
 * Create the toolbar                                                   *
 ***********************************************************************/
void TraceWindow::createToolBars()
{
    QToolBar *toolBar = addToolBar( tr( "Trace" ) );

    toolBar->addSeparator();
    QAction *resetAct = new QAction( QIcon( ":/images/refresh.png" ), tr( "&Reset" ), this );
    // resetAct->setShortcuts(QKeySequence::Refresh);
    resetAct->setStatusTip( tr( "Reset the view" ) );
    connect( resetAct, SIGNAL( triggered() ), this, SLOT( reset() ) );
    toolBar->addAction( resetAct );
    QAction *zoomResetAct =
        new QAction( QIcon( ":/images/zoom_refresh.png" ), tr( "&Reset zoom" ), this );
    zoomResetAct->setShortcuts( QKeySequence::Refresh );
    zoomResetAct->setStatusTip( tr( "Reset the zoom" ) );
    connect( zoomResetAct, SIGNAL( triggered() ), this, SLOT( resetZoom() ) );
    toolBar->addAction( zoomResetAct );

    // Processor popup
    toolBar->addSeparator();
    processorButton = new QToolButton();
    processorButton->setPopupMode( QToolButton::InstantPopup );
    toolBar->addWidget( processorButton );
    processorButtonMenu = new QMenu();
    auto *signalMapper  = new QSignalMapper( this );
    ADD_MENU_ACTION( processorButtonMenu, "All", -1 );
    for ( int i = 0; i < N_procs; i++ )
        ADD_MENU_ACTION( processorButtonMenu, stringf( "Rank %i", i ).c_str(), i );
    connect( signalMapper, SIGNAL( mapped( int ) ), this, SLOT( selectProcessor( int ) ) );
    processorButton->setText( "Processor: All" );
    processorButton->setMenu( processorButtonMenu );
    toolBar->addSeparator();

    // Thread popup
    toolBar->addSeparator();
    threadButton = new QToolButton();
    threadButton->setPopupMode( QToolButton::InstantPopup );
    toolBar->addWidget( threadButton );
    threadButtonMenu = new QMenu();
    signalMapper     = new QSignalMapper( this );
    ADD_MENU_ACTION( threadButtonMenu, "All", -1 );
    for ( int i = 0; i < N_threads; i++ )
        ADD_MENU_ACTION( threadButtonMenu, stringf( "Thread %i", i ).c_str(), i );
    connect( signalMapper, SIGNAL( mapped( int ) ), this, SLOT( selectThread( int ) ) );
    threadButton->setText( "Thread: All" );
    threadButton->setMenu( threadButtonMenu );
    toolBar->addSeparator();

    // Resolution
    QAction *tmp = toolBar->addAction( "Resolution:" );
    tmp->setCheckable( false );
    // tmp->setEnabled(false);
    resolutionBox = new QLineEdit( stringf( "%i", resolution ).c_str() );
    resolutionBox->setMaxLength( 6 );
    resolutionBox->setMaximumWidth( 60 );
    toolBar->addWidget( resolutionBox );
    connect( resolutionBox, SIGNAL( editingFinished() ), this, SLOT( resolutionChanged() ) );
}


/***********************************************************************
 * Helper function to return the global start/stop time                 *
 ***********************************************************************/
std::array<double, 2> TraceWindow::getGlobalTime( const std::vector<TimerResults> &timers )
{
    std::array<double, 2> t_global;
    t_global[0] = 1e100;
    t_global[1] = -1e100;
    for ( const auto &timer : timers ) {
        for ( const auto &trace : timer.trace ) {
            const int N_trace = trace.N_trace;
            if ( N_trace > 0 ) {
                auto start  = trace.start();
                auto stop   = trace.stop();
                t_global[0] = std::min( t_global[0], 1e-9 * start[0] );
                t_global[1] = std::max( t_global[1], 1e-9 * stop[N_trace - 1] );
            }
        }
    }
    return t_global;
}


/***********************************************************************
 * Create/move the trace sliders for the detailed timeline              *
 ***********************************************************************/
void TraceWindow::traceMousePressEvent( QMouseEvent *event )
{
    if ( event->button() == Qt::LeftButton ) {
        traceZoomActive  = true;
        traceZoomLastPos = event->globalPos().x();
        int x            = timerGrid->mapFromGlobal( event->globalPos() ).x();
        double pos       = ( (double) x - timerGrid->rect().left() ) / timerGrid->rect().width();
        zoomBoundaries[0]->move( pos );
        zoomBoundaries[1]->move( pos );
        zoomBoundaries[0]->setVisible( true );
        zoomBoundaries[1]->setVisible( true );
        int x2 = timerPlots[0]->mapFromGlobal( event->globalPos() ).x();
        double t =
            t_current[0] + x2 * ( t_current[1] - t_current[0] ) / timerPlots[0]->rect().width();
        t_zoom[0] = t;
        t_zoom[1] = t;
    }
}
void TraceWindow::traceMouseMoveEvent( QMouseEvent *event )
{
    int x0 = event->globalPos().x();
    if ( !traceZoomActive && abs( x0 - traceZoomLastPos ) < 4 )
        return;
    traceZoomLastPos = x0;
    int x            = timerGrid->mapFromGlobal( event->globalPos() ).x();
    double pos       = ( (double) x - timerGrid->rect().left() ) / timerGrid->rect().width();
    zoomBoundaries[1]->move( pos );
}
void TraceWindow::traceMouseReleaseEvent( QMouseEvent *event )
{
    if ( event->button() == Qt::LeftButton ) {
        traceZoomActive = false;
        int x2          = timerPlots[0]->mapFromGlobal( event->globalPos() ).x();
        double t =
            t_current[0] + x2 * ( t_current[1] - t_current[0] ) / timerPlots[0]->rect().width();
        t_zoom[1] = t;
        if ( t_zoom[1] < t_zoom[0] )
            std::swap( t_zoom[0], t_zoom[1] );
        t_zoom[0] = std::max( t_zoom[0], t_global[0] );
        t_zoom[1] = std::min( t_zoom[1], t_global[1] );
        zoomBoundaries[0]->setVisible( false );
        zoomBoundaries[1]->setVisible( false );
        if ( t_zoom[1] - t_zoom[0] > 0.02 * ( t_current[1] - t_current[0] ) ) {
            t_current = t_zoom;
            updateDisplay( UpdateType::time );
        }
    }
}


/***********************************************************************
 * Run the unit tests                                                   *
 ***********************************************************************/
void TraceWindow::callDefaultTests()
{
    selectProcessor( 0 );
    update();
    selectThread( 0 );
    update();
    resolutionBox->setText( "500" );
    resolutionChanged();
    update();
    resizeDone();
    update();
}
int TraceWindow::runUnitTests()
{
    int N_errors = 0;
    callFun( std::bind( &TraceWindow::update, this ) );
    callFun( std::bind( &TraceWindow::callDefaultTests, this ) );
    callFun( std::bind( &TraceWindow::update, this ) );
    return N_errors;
}
