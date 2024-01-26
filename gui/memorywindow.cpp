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

#include <algorithm>
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
#include "memorywindow.h"


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
MemoryWindow::MemoryWindow( const TimerWindow *parent_ )
    : ThreadedSlotsClass(),
      memory( nullptr ),
      parent( parent_ ),
      N_procs( parent_->N_procs ),
      t_global( getGlobalTime( parent_->d_data.memory ) ),
      selected_rank( -1 )
{
    PROFILE( "MemoryWindow" );
    qApp->processEvents();
    QWidget::setWindowTitle( QString( "Trace results: " ).append( parent->windowTitle() ) );
    t_current = t_global;
    resize( 1200, 800 );


    // Create the memory plot
    memory = new MemoryPlot( this, parent->d_data.memory );

    // Create the layout
    auto *layout = new QVBoxLayout;
    layout->setMargin( 0 );
    layout->setContentsMargins( QMargins( 10, 10, 10, 10 ) );
    layout->setSpacing( 0 );
    layout->addWidget( memory );
    setCentralWidget( new QWidget );
    centralWidget()->setLayout( layout );

    // Create the toolbars
    createToolBars();

    // Create resize event
    resizeTimer.setSingleShot( true );
    connect( &resizeTimer, SIGNAL( timeout() ), SLOT( resizeDone() ) );

    // Plot the data
    reset();

    qApp->processEvents();
}
MemoryWindow::~MemoryWindow()
{
    qApp->processEvents();
    delete processorButtonMenu;
}
void MemoryWindow::closeEvent( QCloseEvent *event )
{
    event->accept();
    parent->memoryWindow.reset(); // Force the deletion of this
}


/***********************************************************************
 * Reset all internal data                                              *
 ***********************************************************************/
void MemoryWindow::reset()
{
    PROFILE( "reset" );
    // Reset t_min and t_max
    t_current = t_global;
    // Update the data
    updateDisplay( UpdateType::all );
}
void MemoryWindow::resetZoom()
{
    PROFILE( "resetZoom" );
    // Reset t_min and t_max
    t_current = t_global;
    // Update the data
    updateDisplay( UpdateType::all );
}


/***********************************************************************
 * Update the display                                                   *
 ***********************************************************************/
void MemoryWindow::updateDisplay( UpdateType update )
{
    PROFILE( "updateDisplay" );
    // Regerate the trace/memory plots if time or processors changed
    if ( ( update & UpdateType::time ) != 0 || ( update & UpdateType::proc ) != 0 ) {
        updateMemory();
    }
    // Start the resize timer to update plots
    resizeTimer.start( 10 );
}


/***********************************************************************
 * Update the memory plot                                               *
 ***********************************************************************/
void MemoryWindow::updateMemory()
{
    PROFILE( "updateMemory" );
    memory->setVisible( true );
    dynamic_cast<MemoryPlot *>( memory )->plot( t_current, selected_rank );
}


/***********************************************************************
 * Resize functions                                                     *
 ***********************************************************************/
void MemoryWindow::resizeEvent( QResizeEvent *e )
{
    resizeTimer.start( 100 );
    QMainWindow::resizeEvent( e );
}
void MemoryWindow::resizeDone() {}
void MemoryWindow::resizeMemory()
{
    PROFILE( "resizeMemory" );
    auto *memplot = dynamic_cast<MemoryPlot *>( memory );
    // int left      = 60;
    // int right     = left + 400;
    // memplot->align( left, right );
    if ( parent->d_data.memory.empty() )
        memplot->setFixedHeight( 45 );
    else
        memplot->setFixedHeight( centralWidget()->height() / 4 );
}


/***********************************************************************
 * Select a processor/thread                                            *
 ***********************************************************************/
void MemoryWindow::selectProcessor( int rank )
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


/***********************************************************************
 * Create the toolbar                                                   *
 ***********************************************************************/
void MemoryWindow::createToolBars()
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
}


/***********************************************************************
 * Helper function to return the global start/stop time                 *
 ***********************************************************************/
std::array<double, 2> MemoryWindow::getGlobalTime( const std::vector<MemoryResults> &data )
{
    std::array<double, 2> t_global;
    t_global[0] = 1e100;
    t_global[1] = -1e100;
    for ( const auto &tmp : data ) {
        for ( auto time : tmp.time ) {
            t_global[0] = std::min( t_global[0], 1e-9 * time );
            t_global[1] = std::max( t_global[1], 1e-9 * time );
        }
    }
    return t_global;
}


/***********************************************************************
 * Run the unit tests                                                   *
 ***********************************************************************/
void MemoryWindow::callDefaultTests()
{
    selectProcessor( 0 );
    update();
    update();
    update();
    resizeDone();
    update();
}
int MemoryWindow::runUnitTests()
{
    int N_errors = 0;
    callFun( std::bind( &MemoryWindow::update, this ) );
    callFun( std::bind( &MemoryWindow::callDefaultTests, this ) );
    callFun( std::bind( &MemoryWindow::update, this ) );
    return N_errors;
}
