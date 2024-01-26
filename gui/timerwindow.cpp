#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QStatusBar>
#include <QtGui>

#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "ProfilerApp.h"
#include "TableValue.h"
#include "memorywindow.h"
#include "timerwindow.h"
#include "tracewindow.h"

extern "C" {
#include <cassert>
}


// Function to convert the thread list to a string
std::string threadString( const std::vector<int>& x )
{
    if ( x.empty() )
        return std::string();
    if ( x.size() > 1 )
        return std::string( "multiple" );
    std::string s = stringf( "%i", x[0] );
    for ( size_t i = 1; i < x.size(); i++ )
        s += stringf( "%s, %i", s.c_str(), x[i] );
    return s;
}


// Function to search and return the timer for an id
static const TimerResults& findTimer( const std::vector<TimerResults>& timers, id_struct id )
{
    for ( const auto& timer : timers ) {
        if ( timer.id == id )
            return timer;
    }
    printf( "timer not found\n" );
    static TimerResults null_timer;
    return null_timer;
}


// Get a list of active traces
inline std::vector<id_struct> getActive( const TraceResults& trace )
{
    std::vector<id_struct> active( trace.N_active );
    for ( size_t i = 0; i < active.size(); i++ )
        active[i] = trace.active[i];
    std::sort( active.begin(), active.end() );
    return active;
}


// Do we want to keep the trace
inline bool keepTrace( const TraceSummary& trace, const std::vector<id_struct>& callList,
    const bool keep_subfunctions )
{
    bool keep = true;
    for ( const auto& id : callList ) {
        bool found = trace.id == id;
        for ( const auto& id2 : trace.active )
            found = found || id == id2;
        keep = keep && found;
    }
    if ( keep && !keep_subfunctions ) {
        keep = keep && trace.active.size() <= callList.size();
        if ( trace.active.size() == callList.size() ) {
            // We should not find id in the call list
            bool found = false;
            for ( const auto& i : callList )
                found = found || trace.id == i;
            keep = keep && !found;
        }
    }
    return keep;
}


// Compute the min of a vector
template<class TYPE>
TYPE min( const std::vector<TYPE>& x )
{
    TYPE y = std::numeric_limits<TYPE>::max();
    for ( size_t i = 0; i < x.size(); i++ )
        y = std::min( y, x[i] );
    return y;
}


// Compute the max of a vector
template<class TYPE>
TYPE max( const std::vector<TYPE>& x )
{
    TYPE y = std::numeric_limits<TYPE>::min();
    for ( size_t i = 0; i < x.size(); i++ )
        y = std::max( y, x[i] );
    return y;
}


// Compute the sum of a vector
template<class TYPE>
TYPE sum( const std::vector<TYPE>& x )
{
    TYPE y = 0;
    for ( size_t i = 0; i < x.size(); i++ )
        y += x[i];
    return y;
}


// Compute the mean of a vector
template<class TYPE>
double mean( const std::vector<TYPE>& x )
{
    return static_cast<double>( sum( x ) ) / static_cast<double>( x.size() );
}


// Function to add an action (with connection) to a menu
#define ADD_MENU_ACTION( menu, string, arg )                                   \
    do {                                                                       \
        QAction* action = new QAction( string, this );                         \
        connect( action, SIGNAL( triggered() ), signalMapper, SLOT( map() ) ); \
        signalMapper->setMapping( action, arg );                               \
        menu->addAction( action );                                             \
    } while ( 0 )


#define ASSERT( EXP )                                                                     \
    do {                                                                                  \
        if ( !( EXP ) ) {                                                                 \
            std::stringstream stream;                                                     \
            stream << "Failed assertion: " << #EXP << " " << __FILE__ << " " << __LINE__; \
            throw std::logic_error( stream.str() );                                       \
        }                                                                                 \
    } while ( 0 )


/***********************************************************************
 * ActiveStruct                                                         *
 ***********************************************************************/
ActiveStruct::ActiveStruct() : hash( 0 ) {}
ActiveStruct::ActiveStruct( const std::vector<id_struct>& ids ) : hash( 0 ), active( ids )
{
    constexpr uint64_t C = 0x61C8864680B583EBull;
    for ( auto id : ids )
        hash ^= C * static_cast<uint32_t>( id );
}
bool ActiveStruct::operator==( const ActiveStruct& rhs ) const { return hash == rhs.hash; }


/***********************************************************************
 * Constructor/destructor                                               *
 ***********************************************************************/
TimerWindow::TimerWindow()
    : ThreadedSlotsClass(),
      mainMenu( nullptr ),
      timerTable( nullptr ),
      callLineText( nullptr ),
      backButton( nullptr ),
      processorButton( nullptr )
{
    PROFILE( "TimerWindow constructor" );
    QWidget::setWindowTitle( QString( "load_timer" ) );

    // Create the timer table
    timerTable = new QTableWidget;
    timerTable->hide();
    connect( timerTable, SIGNAL( cellDoubleClicked( int, int ) ), this,
        SLOT( cellSelected( int, int ) ) );

    // Create the call line
    callLineText = new QLineEdit( QString( "" ) );
    callLineText->setFrame( false );
    callLineText->setReadOnly( true );
    QString bgColorName = palette().color( QPalette::Normal, QPalette::Window ).name();
    QString strStyleSheet =
        QString( "QLineEdit {background-color: " ).append( bgColorName ).append( "}" );
    callLineText->setStyleSheet( strStyleSheet );

    // Back button
    backButton = new QPushButton( "Back" );
    connect( backButton, SIGNAL( released() ), this, SLOT( backButtonPressed() ) );
    backButton->hide();

    // Create the load balance chart
    loadBalance = new LoadBalance( this );
    loadBalance->hide();

    // Create the layout
    auto* layout_callLine = new QHBoxLayout;
    layout_callLine->addWidget( callLineText );
    layout_callLine->addWidget( backButton );
    QWidget* widget_callLine = new QWidget;
    widget_callLine->setLayout( layout_callLine );

    auto* layout = new QVBoxLayout;
    layout->setMargin( 0 );
    layout->setContentsMargins( QMargins( 0, 0, 0, 0 ) );
    layout->setSpacing( 0 );
    layout->addWidget( widget_callLine );
    layout->addWidget( timerTable );
    layout->addWidget( loadBalance );
    setCentralWidget( new QWidget );
    centralWidget()->setLayout( layout );

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    // Create resize event
    resizeTimer.setSingleShot( true );
    connect( &resizeTimer, SIGNAL( timeout() ), SLOT( resizeDone() ) );

    setUnifiedTitleAndToolBarOnMac( true );
    reset();
    updateDisplay();
    qApp->processEvents();
}
TimerWindow::~TimerWindow()
{
    // Close the file
    close();
    // Disconnect signals created by createActions
    disconnect( openAct, SIGNAL( triggered() ), this, SLOT( open() ) );
    disconnect( closeAct, SIGNAL( triggered() ), this, SLOT( close() ) );
    disconnect( resetAct, SIGNAL( triggered() ), this, SLOT( reset() ) );
    disconnect( exclusiveAct, SIGNAL( triggered() ), this, SLOT( exclusiveFun() ) );
    disconnect( subfunctionsAct, SIGNAL( triggered() ), this, SLOT( subfunctionFun() ) );
    disconnect( exitAct, SIGNAL( triggered() ), this, SLOT( exit() ) );
    disconnect( aboutAct, SIGNAL( triggered() ), this, SLOT( about() ) );
    disconnect( savePerformanceTimers, SIGNAL( triggered() ), this, SLOT( savePerformance() ) );
    disconnect( runUnitTestAction, SIGNAL( triggered() ), this, SLOT( runUnitTestsSlot() ) );
    // Disconnect signals created by createToolbars
    disconnect( exclusiveButton, SIGNAL( released() ), this, SLOT( exclusiveFun() ) );
    disconnect( subfunctionButton, SIGNAL( released() ), this, SLOT( subfunctionFun() ) );
    disconnect( traceButton, SIGNAL( released() ), this, SLOT( traceFun() ) );
    // Delete objects
    delete timerTable;
    delete mainMenu;
}
void TimerWindow::closeEvent( QCloseEvent* event )
{
    writeSettings();
    close();
    event->accept();
}
void TimerWindow::close()
{
    d_data = TimerMemoryResults();
    d_dataTimer.clear();
    d_dataTrace.clear();
    traceWindow.reset();
    memoryWindow.reset();
    QWidget::setWindowTitle( QString( "load_timer" ) );
    reset();
}
void TimerWindow::exit() { qApp->quit(); }


/***********************************************************************
 * Reset the view                                                       *
 ***********************************************************************/
void TimerWindow::reset()
{
    if ( loadBalance != nullptr )
        loadBalance->plot( std::vector<float>() );
    callList.clear();
    inclusiveTime       = true;
    includeSubfunctions = true;
    traceToolbar->setVisible( false );
    memoryToolbar->setVisible( false );
    updateDisplay();
}


/***********************************************************************
 * Help                                                                 *
 ***********************************************************************/
void TimerWindow::about()
{
    QMessageBox::about( this, tr( "About Application" ),
        tr( "This is the graphical user interface for viewing "
            "the timer data from the ProfilerApp." ) );
}


/***********************************************************************
 * Save the timers for the application                                  *
 ***********************************************************************/
void TimerWindow::savePerformance()
{
    std::string filename = QFileDialog::getSaveFileName(
        this, "Name of file to save", lastPath.c_str(), "*.0.timer *.1.timer" )
                               .toStdString();
    if ( !filename.empty() ) {
        size_t i = filename.rfind( '.', filename.rfind( ".timer" ) - 1 );
        filename = filename.substr( 0, i );
        PROFILE_SAVE( filename );
    }
}


/***********************************************************************
 * Load timer data                                                      *
 ***********************************************************************/
void TimerWindow::open()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Select the timer file", lastPath.c_str(), "*.0.timer *.1.timer" );
    loadFile( filename.toStdString() );
}
void TimerWindow::loadFile( std::string filename, bool showFailure )
{
    PROFILE( "loadFile" );
    if ( !filename.empty() ) {
        // Clear existing data
        close();
        // Get the filename and path
        bool global = filename.rfind( ".0.timer" ) != std::string::npos;
        int pos     = filename.rfind( '.', filename.rfind( ".timer" ) - 1 );
        filename    = filename.substr( 0, pos );
        pos         = -1;
        if ( filename.rfind( (char) 47 ) != std::string::npos )
            pos = std::max<int>( pos, filename.rfind( (char) 47 ) );
        if ( filename.rfind( (char) 92 ) != std::string::npos )
            pos = std::max<int>( pos, filename.rfind( (char) 92 ) );
        std::string path, file;
        if ( pos != -1 ) {
            path     = filename.substr( 0, pos );
            file     = filename.substr( pos + 1 );
            lastPath = path;
        }
        std::string title = file + " (" + path + ")";
        QWidget::setWindowTitle( title.c_str() );
        {
            PROFILE( "ProfilerApp::load" );
            try {
                // Load the timer data
                d_data = ProfilerApp::load( filename, -1, global );
            } catch ( std::exception& e ) {
                if ( showFailure ) {
                    std::string msg = e.what();
                    QMessageBox::information( this, tr( "Error loading file" ), tr( msg.c_str() ) );
                }
                return;
            } catch ( ... ) {
                if ( showFailure ) {
                    QMessageBox::information(
                        this, tr( "Error loading file" ), tr( "Caught unknown exception" ) );
                }
                return;
            }
        }
        // Remove traces that don't have any calls
        for ( auto& timer : d_data.timers ) {
            std::vector<TraceResults>& trace = timer.trace;
            for ( int64_t j = trace.size() - 1; j >= 0; j-- ) {
                if ( trace[j].N == 0 ) {
                    std::swap( trace[j], trace[trace.size() - 1] );
                    trace.resize( trace.size() - 1 );
                }
            }
            std::sort( trace.begin(), trace.end(),
                []( const auto& i, const auto& j ) { return ( i.id < j.id ); } );
        }
        // Get the number of processors/threads
        N_procs       = d_data.N_procs;
        N_threads     = 0;
        selected_rank = -1;
        for ( auto& timer : d_data.timers ) {
            for ( auto& j : timer.trace )
                N_threads = std::max<int>( N_threads, j.thread + 1 );
        }
        // Get the global summary data
        d_dataTimer.clear();
        d_dataTimer.resize( d_data.timers.size() );
        d_dataTrace.clear();
        d_dataTrace.reserve( 1000 );
        for ( size_t i = 0; i < d_data.timers.size(); i++ ) {
            TimerSummary& timer = d_dataTimer[i];
            timer.id            = d_data.timers[i].id;
            timer.message       = d_data.timers[i].message;
            timer.file          = d_data.timers[i].file;
            timer.line          = d_data.timers[i].line;
            timer.threads.clear();
            timer.N.resize( N_procs, 0 );
            timer.min.resize( N_procs, 1e30 );
            timer.max.resize( N_procs, 0 );
            timer.tot.resize( N_procs, 0 );
            timer.trace.clear();
            for ( auto& t0 : d_data.timers[i].trace ) {
                int index   = -1;
                auto active = getActive( t0 );
                for ( size_t k = 0; k < timer.trace.size(); k++ ) {
                    if ( timer.trace[k]->active == active ) {
                        index = static_cast<int>( k );
                        break;
                    }
                }
                if ( index == -1 ) {
                    index    = static_cast<int>( timer.trace.size() );
                    size_t k = d_dataTrace.size();
                    d_dataTrace.emplace_back( std::make_unique<TraceSummary>() );
                    d_dataTrace[k]->id     = t0.id;
                    d_dataTrace[k]->active = active;
                    d_dataTrace[k]->N.resize( N_procs, 0 );
                    d_dataTrace[k]->min.resize( N_procs, 1e30 );
                    d_dataTrace[k]->max.resize( N_procs, 0 );
                    d_dataTrace[k]->tot.resize( N_procs, 0 );
                    timer.trace.push_back( d_dataTrace[k].get() );
                }
                auto* trace = const_cast<TraceSummary*>( timer.trace[index] );
                int rank    = t0.rank;
                trace->threads.insert( t0.thread );
                trace->N[rank] += t0.N;
                trace->min[rank] = std::min( trace->min[rank], 1e-9f * t0.min );
                trace->max[rank] = std::max( trace->max[rank], 1e-9f * t0.max );
                trace->tot[rank] += 1e-9f * t0.tot;
            }
            std::set<int> ids;
            for ( size_t j = 0; j < timer.trace.size(); j++ ) {
                const TraceSummary* trace = timer.trace[j];
                for ( int k = 0; k < N_procs; k++ ) {
                    timer.N[k] += trace->N[k];
                    timer.min[k] = std::min( timer.min[k], trace->min[k] );
                    timer.max[k] = std::max( timer.max[k], trace->max[k] );
                    timer.tot[k] += trace->tot[k];
                }
                ids.insert( trace->threads.begin(), trace->threads.end() );
            }
            timer.threads = std::vector<int>( ids.begin(), ids.end() );
        }
        traceToolbar->setVisible( hasTraceData() );
        memoryToolbar->setVisible( !d_data.memory.empty() );
        // Set min for any ranks with zero calls to zero
        for ( auto& time : d_dataTimer ) {
            for ( int k = 0; k < N_procs; k++ ) {
                if ( time.N[k] == 0 )
                    time.min[k] = 0;
            }
            for ( auto& trace : d_dataTrace ) {
                for ( int k = 0; k < N_procs; k++ ) {
                    if ( trace->N[k] == 0 )
                        trace->min[k] = 0;
                }
            }
        }
        // Update the display
        printf( "%s loaded successfully\n", filename.c_str() );
        if ( N_procs > 1 ) {
            processorButtonMenu.reset( new QMenu() );
            auto* signalMapper = new QSignalMapper( this );
            ADD_MENU_ACTION( processorButtonMenu, "Average", -1 );
            ADD_MENU_ACTION( processorButtonMenu, "Minimum", -2 );
            ADD_MENU_ACTION( processorButtonMenu, "Maximum", -3 );
            if ( N_procs < 100 ) {
                for ( int j = 0; j < N_procs; j++ )
                    ADD_MENU_ACTION( processorButtonMenu, stringf( "Rank %i", j ).c_str(), j );
            } else {
                const int ranks_per_menu = 250;
                QMenu* rank_menu         = nullptr;
                for ( int j = 0; j < N_procs; j++ ) {
                    if ( j % ranks_per_menu == 0 ) {
                        auto name = stringf( "Ranks %i-%i", j, j + ranks_per_menu - 1 );
                        rank_menu = processorButtonMenu->addMenu( name.c_str() );
                    }
                    ADD_MENU_ACTION( rank_menu, stringf( "%5i", j ).c_str(), j );
                }
            }
            connect( signalMapper, SIGNAL( mapped( int ) ), this, SLOT( selectProcessor( int ) ) );
            selectProcessor( -1 );
            processorButton->setMenu( processorButtonMenu.get() );
        }
        updateDisplay();
    }
}


/***********************************************************************
 * Update the display                                                   *
 ***********************************************************************/
bool TimerWindow::hasTraceData() const
{
    bool traceData = false;
    for ( const auto& timer : d_data.timers ) {
        for ( const auto& j : timer.trace )
            traceData = traceData || j.N_trace > 0;
    }
    return traceData;
}


/***********************************************************************
 * Update the display                                                   *
 ***********************************************************************/
template<class TYPE>
inline TYPE getTableData( const std::vector<TYPE>& x, int rank )
{
    TYPE y;
    if ( rank == -1 ) {
        y = static_cast<TYPE>( mean( x ) );
    } else if ( rank == -2 ) {
        y = static_cast<TYPE>( min( x ) );
    } else if ( rank == -3 ) {
        y = static_cast<TYPE>( max( x ) );
    } else {
        y = x[rank];
    }
    return y;
}
void TimerWindow::updateDisplay()
{
    PROFILE( "updateDisplay" );
    auto current_timers = getTimers();
    if ( d_data.timers.empty() ) {
        timerTable->hide();
        callLineText->hide();
        backButton->hide();
        loadBalance->hide();
        processorToolbar->setVisible( false );
        exclusiveToolbar->setVisible( false );
        subfunctionToolbar->setVisible( false );
        return;
    }
    if ( N_procs > 1 )
        processorToolbar->setVisible( true );
    exclusiveToolbar->setVisible( true );
    subfunctionToolbar->setVisible( true );
    if ( includeSubfunctions ) {
        subfunctionButton->setText( "Hide Subfunctions" );
        subfunctionButton->setFlat( false );
    } else {
        subfunctionButton->setText( "Show Subfunctions" );
        subfunctionButton->setFlat( true );
    }
    if ( inclusiveTime ) {
        exclusiveButton->setText( "Exclusive Time" );
        exclusiveButton->setFlat( false );
    } else {
        exclusiveButton->setText( "Inclusive Time" );
        exclusiveButton->setFlat( true );
    }
    // Set the call line
    if ( !callList.empty() ) {
        const auto& timer      = findTimer( d_data.timers, callList[0] );
        std::string call_stack = timer.message;
        for ( size_t i = 1; i < callList.size(); i++ ) {
            const auto& timer2 = findTimer( d_data.timers, callList[i] );
            call_stack += " -> " + std::string( timer2.message );
        }
        callLineText->setText( call_stack.c_str() );
        callLineText->show();
        backButton->show();
    } else {
        callLineText->setText( "" );
        callLineText->hide();
        backButton->hide();
    }
    // Fill the table data
    QStringList TableHeader;
    TableHeader << "id"
                << "Message"
                << "Filename"
                << "Thread"
                << "Line"
                << "N calls"
                << "min time"
                << "max time"
                << "total time"
                << "% time";
    timerTable->clear();
    timerTable->setRowCount( 0 );
    timerTable->setRowCount( current_timers.size() );
    timerTable->setColumnCount( 11 );
    timerTable->QTableView::setColumnHidden( 0, true );
    timerTable->setColumnWidth( 1, 200 );
    timerTable->setColumnWidth( 2, 200 );
    timerTable->setColumnWidth( 3, 80 );
    timerTable->setColumnWidth( 4, 80 );
    timerTable->setColumnWidth( 5, 80 );
    timerTable->setColumnWidth( 6, 95 );
    timerTable->setColumnWidth( 7, 95 );
    timerTable->setColumnWidth( 8, 95 );
    timerTable->setColumnWidth( 9, 85 );
    timerTable->setHorizontalHeaderLabels( TableHeader );
    timerTable->verticalHeader()->setVisible( false );
    timerTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
    size_t N_timers = current_timers.size();
    std::vector<int> N_data( N_timers );
    std::vector<double> min_data( N_timers ), max_data( N_timers ), tot_data( N_timers );
    for ( size_t i = 0; i < N_timers; i++ ) {
        N_data[i]   = getTableData( current_timers[i]->N, selected_rank );
        min_data[i] = getTableData( current_timers[i]->min, selected_rank );
        max_data[i] = getTableData( current_timers[i]->max, selected_rank );
        tot_data[i] = getTableData( current_timers[i]->tot, selected_rank );
    }
    double tot_max = d_data.walltime;
    if ( !callList.empty() )
        tot_max = max( tot_data );
    std::vector<double> ratio( N_timers, 0 );
    for ( size_t i = 0; i < N_timers; i++ )
        ratio[i] = tot_data[i] / tot_max;
    for ( size_t i = 0; i < N_timers; i++ ) {
        timerTable->setRowHeight( i, 25 );
        auto timer   = current_timers[i].get();
        auto id      = new QTableWidgetItem( timer->id.string().c_str() );
        auto message = new QTableWidgetItem( timer->message.c_str() );
        auto file    = new QTableWidgetItem( timer->file.c_str() );
        auto threads = new QTableWidgetItem( threadString( timer->threads ).c_str() );
        auto line    = new TableValue( timer->line, "%i" );
        auto N       = new TableValue( N_data[i], "%i" );
        auto min     = new TableValue( min_data[i], "%0.3e" );
        auto max     = new TableValue( max_data[i], "%0.3e" );
        auto total   = new TableValue( tot_data[i], "%0.3e" );
        auto percent = new TableValue( 100 * ratio[i], "%0.2f" );
        id->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        message->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        file->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        threads->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        line->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        N->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        min->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        max->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        total->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        percent->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        timerTable->setItem( i, 0, id );
        timerTable->setItem( i, 1, message );
        timerTable->setItem( i, 2, file );
        timerTable->setItem( i, 3, threads );
        timerTable->setItem( i, 4, line );
        timerTable->setItem( i, 5, N );
        timerTable->setItem( i, 6, min );
        timerTable->setItem( i, 7, max );
        timerTable->setItem( i, 8, total );
        timerTable->setItem( i, 9, percent );
    }
    timerTable->setSortingEnabled( true );
    timerTable->sortItems( 10, Qt::DescendingOrder );
    timerTable->show();
    // Update the load balance plot
    if ( N_procs > 1 && !current_timers.empty() ) {
        // Plot the timer with the largest time
        size_t index = 0;
        double value = 0;
        for ( size_t i = 0; i < N_timers; i++ ) {
            if ( tot_data[i] > value ) {
                value = tot_data[i];
                index = i;
            }
        }
        loadBalance->plot( current_timers[index]->tot );
        loadBalance->setFixedHeight( centralWidget()->height() / 4 );
        loadBalance->show();
    } else {
        loadBalance->hide();
    }
}


/***********************************************************************
 * Get the timers of interest                                           *
 ***********************************************************************/
std::vector<std::unique_ptr<TimerSummary>> TimerWindow::getTimers() const
{
    PROFILE( "getTimers" );
    std::vector<std::unique_ptr<TimerSummary>> timers;
    timers.reserve( d_data.timers.size() );
    // Get the timers of interest
    for ( const auto& i : d_dataTimer ) {
        auto timer     = std::make_unique<TimerSummary>();
        timer->id      = i.id;
        timer->message = i.message;
        timer->file    = i.file;
        timer->line    = i.line;
        if ( callList.empty() ) {
            // Special case where we want all timers/trace results
            timer->trace = i.trace;
        } else {
            // Get the timers that have all of the call hierarchy
            timer->trace.resize( 0 );
            timer->trace.reserve( i.trace.size() );
            for ( auto j : i.trace ) {
                const TraceSummary* trace = j;
                ASSERT( trace != nullptr );
                if ( keepTrace( *trace, callList, true ) )
                    timer->trace.push_back( j );
            }
        }
        if ( !timer->trace.empty() )
            timers.push_back( std::move( timer ) );
    }
    // Keep only the traces that are directly inherited from the current call hierarchy
    if ( !includeSubfunctions ) {
        PROFILE( "getTimers-hideSubfunctions", 1 );
        std::map<id_struct, int> id_map;
        for ( size_t i = 0; i < d_dataTimer.size(); i++ )
            id_map.insert( std::pair<id_struct, int>( d_dataTimer[i].id, (int) i ) );
        std::vector<bool> tmp0( id_map.size(), 0 );
        for ( size_t i = 0; i < timers.size(); i++ )
            tmp0[id_map[timers[i]->id]] = true;
        std::vector<bool> keep( timers.size(), false );
        for ( size_t i = 0; i < timers.size(); i++ ) {
            auto timer  = timers[i].get();
            auto trace2 = timer->trace;
            timer->trace.resize( 0 );
            for ( auto& t : trace2 ) {
                int tmp2 = 0;
                for ( const auto& k : t->active ) {
                    if ( tmp0[id_map[k]] )
                        tmp2++;
                }
                if ( tmp2 <= 1 ) {
                    keep[i] = true;
                    timer->trace.push_back( t );
                }
            }
        }
        size_t k = 0;
        for ( size_t i = 0; i < timers.size(); i++ ) {
            if ( keep[i] ) {
                std::swap( timers[k], timers[i] );
                k++;
            }
        }
        timers.resize( k );
    }
    // Compute the timer statistics
    for ( auto& timer : timers ) {
        timer->N.resize( N_procs, 0 );
        timer->min.resize( N_procs, 1e100 );
        timer->max.resize( N_procs, 0.0 );
        timer->tot.resize( N_procs, 0.0 );
        std::set<int> threads;
        for ( const auto& trace : timer->trace ) {
            threads.insert( trace->threads.begin(), trace->threads.end() );
            for ( int k = 0; k < N_procs; k++ ) {
                timer->N[k] += trace->N[k];
                timer->min[k] = std::min<double>( timer->min[k], trace->min[k] );
                timer->max[k] = std::max<double>( timer->max[k], trace->max[k] );
                timer->tot[k] += trace->tot[k];
            }
        }
        timer->threads = std::vector<int>( threads.begin(), threads.end() );
    }
    // Update the timers to remove the time from sub-timers if necessary
    if ( !inclusiveTime ) {
        PROFILE( "getTimers-removeSubtimers", 1 );
        constexpr uint64_t C = 0x61C8864680B583EBull;
        for ( size_t i1 = 0; i1 < timers.size(); i1++ ) {
            for ( size_t j1 = 0; j1 < timers[i1]->trace.size(); j1++ ) {
                auto h1 = timers[i1]->trace[j1]->active.getHash();
                h1 ^= C * static_cast<uint32_t>( timers[i1]->id );
                for ( size_t i2 = 0; i2 < timers.size(); i2++ ) {
                    for ( size_t j2 = 0; j2 < timers[i2]->trace.size(); j2++ ) {
                        auto h2 = timers[i2]->trace[j2]->active.getHash();
                        if ( h1 == h2 ) {
                            const auto& tot = timers[i2]->trace[j2]->tot;
                            for ( int k = 0; k < N_procs; k++ )
                                timers[i1]->tot[k] -= tot[k];
                        }
                    }
                }
            }
        }
    }
    return timers;
}


/***********************************************************************
 * Select a cell in the table                                           *
 ***********************************************************************/
void TimerWindow::cellSelected( int row, int col )
{
    if ( col != 1 )
        return;
    id_struct id( timerTable->item( row, 0 )->text().toStdString() );
    bool found = false;
    for ( auto& i : callList )
        found = found || i == id;
    if ( !found )
        callList.push_back( id );
    updateDisplay();
}


/***********************************************************************
 * Back button                                                          *
 ***********************************************************************/
void TimerWindow::backButtonPressed()
{
    if ( callList.empty() )
        return;
    callList.pop_back();
    updateDisplay();
}


/***********************************************************************
 * Select a processor                                                   *
 ***********************************************************************/
void TimerWindow::selectProcessor( int rank )
{
    selected_rank = rank;
    if ( rank == -1 ) {
        processorButton->setText( "Average" );
    } else if ( rank == -2 ) {
        processorButton->setText( "Minimum" );
    } else if ( rank == -3 ) {
        processorButton->setText( "Maximum" );
    } else {
        processorButton->setText( stringf( "Rank %i", rank ).c_str() );
    }
    updateDisplay();
}


/***********************************************************************
 * Select a processor                                                   *
 ***********************************************************************/
void TimerWindow::exclusiveFun()
{
    inclusiveTime = !inclusiveTime;
    updateDisplay();
}


/***********************************************************************
 * Select a processor                                                   *
 ***********************************************************************/
void TimerWindow::subfunctionFun()
{
    includeSubfunctions = !includeSubfunctions;
    updateDisplay();
}


/***********************************************************************
 * Create the actions                                                   *
 ***********************************************************************/
void TimerWindow::createActions()
{
    openAct = new QAction( QIcon( ":/images/open.png" ), tr( "&Open..." ), this );
    openAct->setShortcuts( QKeySequence::Open );
    openAct->setStatusTip( tr( "Open an existing file" ) );
    connect( openAct, SIGNAL( triggered() ), this, SLOT( open() ) );

    closeAct = new QAction( QIcon( ":/images/new.png" ), tr( "&Close" ), this );
    closeAct->setShortcuts( QKeySequence::Close );
    closeAct->setStatusTip( tr( "Create a new file" ) );
    connect( closeAct, SIGNAL( triggered() ), this, SLOT( close() ) );

    resetAct = new QAction( QIcon( ":/images/refresh.png" ), tr( "&Reset" ), this );
    resetAct->setShortcuts( QKeySequence::Refresh );
    resetAct->setStatusTip( tr( "Reset the view" ) );
    connect( resetAct, SIGNAL( triggered() ), this, SLOT( reset() ) );

    exclusiveAct = new QAction( tr( "&Exclusive Time" ), this );
    exclusiveAct->setStatusTip( tr( "Exclusive Time" ) );
    connect( exclusiveAct, SIGNAL( triggered() ), this, SLOT( exclusiveFun() ) );

    subfunctionsAct = new QAction( tr( "&Hide Subfunctions" ), this );
    subfunctionsAct->setStatusTip( tr( "Hide Subfunctions" ) );
    connect( subfunctionsAct, SIGNAL( triggered() ), this, SLOT( subfunctionFun() ) );

    exitAct = new QAction( tr( "E&xit" ), this );
    exitAct->setShortcuts( QKeySequence::Quit );
    exitAct->setStatusTip( tr( "Exit the application" ) );
    connect( exitAct, SIGNAL( triggered() ), this, SLOT( exit() ) );

    aboutAct = new QAction( tr( "&About" ), this );
    aboutAct->setStatusTip( tr( "Show the application's About box" ) );
    connect( aboutAct, SIGNAL( triggered() ), this, SLOT( about() ) );

    savePerformanceTimers = new QAction( tr( "&Save performance" ), this );
    savePerformanceTimers->setStatusTip( tr( "Save performance timers" ) );
    connect( savePerformanceTimers, SIGNAL( triggered() ), this, SLOT( savePerformance() ) );

    runUnitTestAction = new QAction( tr( "&Run unit tests" ), this );
    runUnitTestAction->setStatusTip( tr( "Run unit tests" ) );
    connect( runUnitTestAction, SIGNAL( triggered() ), this, SLOT( runUnitTestsSlot() ) );
}

void TimerWindow::createMenus()
{
    mainMenu = new QMenuBar( nullptr );
    fileMenu = mainMenu->addMenu( tr( "&File" ) );
    fileMenu->addAction( openAct );
    fileMenu->addAction( closeAct );
    fileMenu->addAction( resetAct );
    // fileMenu->addAction(saveAct);
    // fileMenu->addAction(saveAsAct);
    // fileMenu->addSeparator();
    fileMenu->addAction( exitAct );

    // editMenu = mainMenu->addMenu(tr("&Edit"));
    // editMenu->addAction(cutAct);
    // editMenu->addAction(copyAct);
    // editMenu->addAction(pasteAct);

    mainMenu->addSeparator();

    helpMenu = mainMenu->addMenu( tr( "&Help" ) );
    helpMenu->addAction( aboutAct );

    // Create developer menu on rhs
    auto* developer_bar = new QMenuBar( this );
    QMenu* developer    = developer_bar->addMenu( tr( "&Developer" ) );
    developer->addAction( savePerformanceTimers );
    developer->addAction( runUnitTestAction );
    mainMenu->setCornerWidget( developer_bar );

// In Ubuntu 14.04 with qt5 the window's menu bar goes missing entirely
// if the user is running any desktop environment other than Unity
// (in which the faux single-menubar appears). The user has a
// workaround, to remove the appmenu-qt5 package, but that is
// awkward and the problem is so severe that it merits disabling
// the system menubar integration altogether. Like this:
#if defined( Q_OS_LINUX ) && QT_VERSION >= 0x050000
    mainMenu->setNativeMenuBar( false );      // fix #1039
    developer_bar->setNativeMenuBar( false ); // fix #1039
#endif

    setMenuBar( mainMenu );
}

void TimerWindow::createToolBars()
{
    fileToolBar = addToolBar( tr( "File" ) );
    fileToolBar->addAction( closeAct );
    fileToolBar->addAction( openAct );
    fileToolBar->addSeparator();
    fileToolBar->addAction( resetAct );
    // Processor popup
    processorButton = new QToolButton();
    processorButton->setPopupMode( QToolButton::InstantPopup );
    processorToolbar = fileToolBar->addWidget( processorButton );
    // Exclusive time
    exclusiveButton = new QPushButton( "Exclusive Time" );
    connect( exclusiveButton, SIGNAL( released() ), this, SLOT( exclusiveFun() ) );
    exclusiveToolbar = fileToolBar->addWidget( exclusiveButton );
    // Hide subfunctions
    subfunctionButton = new QPushButton( "Hide Subfunctions" );
    connect( subfunctionButton, SIGNAL( released() ), this, SLOT( subfunctionFun() ) );
    subfunctionToolbar = fileToolBar->addWidget( subfunctionButton );
    // Hide subfunctions
    traceButton  = new QPushButton( "Plot Trace Data" );
    memoryButton = new QPushButton( "Plot Memory Data" );
    connect( traceButton, SIGNAL( released() ), this, SLOT( traceFun() ) );
    connect( memoryButton, SIGNAL( released() ), this, SLOT( memoryFun() ) );
    traceToolbar  = fileToolBar->addWidget( traceButton );
    memoryToolbar = fileToolBar->addWidget( memoryButton );


    // editToolBar = addToolBar(tr("Edit"));
    // editToolBar->addAction(cutAct);
    // editToolBar->addAction(copyAct);
    // editToolBar->addAction(pasteAct);
}

void TimerWindow::createStatusBar() { statusBar()->showMessage( tr( "Ready" ) ); }

void TimerWindow::readSettings()
{
    QSettings settings( "Trolltech", "Application Example" );
    QPoint pos = settings.value( "pos", QPoint( 200, 200 ) ).toPoint();
    QSize size = settings.value( "size", QSize( 400, 400 ) ).toSize();
    resize( size );
    move( pos );
}

void TimerWindow::writeSettings()
{
    QSettings settings( "Trolltech", "Application Example" );
    settings.setValue( "pos", pos() );
    settings.setValue( "size", size() );
}


/***********************************************************************
 * Resize functions                                                     *
 ***********************************************************************/
void TimerWindow::resizeEvent( QResizeEvent* e )
{
    resizeTimer.start( 500 );
    QMainWindow::resizeEvent( e );
}
void TimerWindow::resizeDone() { loadBalance->setFixedHeight( centralWidget()->height() / 4 ); }


/***********************************************************************
 * Create the trace window                                              *
 ***********************************************************************/
void TimerWindow::traceFun()
{
    traceWindow.reset( new TraceWindow( this ) );
#if defined( Q_OS_SYMBIAN )
    traceWindow->showMaximized();
#else
    traceWindow->show();
#endif
}


/***********************************************************************
 * Create the memory window                                             *
 ***********************************************************************/
void TimerWindow::memoryFun()
{
    memoryWindow.reset( new MemoryWindow( this ) );
#if defined( Q_OS_SYMBIAN )
    memoryWindow->showMaximized();
#else
    memoryWindow->show();
#endif
}


/***********************************************************************
 * Run the unit tests                                                   *
 ***********************************************************************/
void TimerWindow::runUnitTestsSlot()
{
    // Get the filename to test
    QString filename = QFileDialog::getOpenFileName(
        this, "Select the timer file to test", lastPath.c_str(), "*.0.timer *.1.timer" );
    // Run the unit tests
    bool pass = runUnitTests( filename.toStdString() );
    if ( pass ) {
        QMessageBox::information( this, tr( "All tests passed" ), tr( "All tests passed" ) );
    } else {
        QMessageBox::information( this, tr( "Some tests failed" ), tr( "Some tests failed" ) );
    }
}
void TimerWindow::callLoadFile()
{
    loadFile( unitTestFilename, false );
    qApp->processEvents();
}
void TimerWindow::callSelectCell()
{
    cellSelected( 0, 1 );
    update();
    subfunctionFun();
    update();
    exclusiveFun();
    update();
    subfunctionFun();
    update();
    exclusiveFun();
    update();
    backButtonPressed();
}
void TimerWindow::closeTrace() { traceWindow->close(); }
bool TimerWindow::runUnitTests( const std::string& filename )
{
    // Try to load the file
    unitTestFilename = filename;
    callFun( std::bind( &TimerWindow::callLoadFile, this ) );
    if ( d_dataTimer.empty() ) {
        printf( "   Failed to load file %s\n", filename.c_str() );
        return false;
    }
    // Select the first cell
    callFun( std::bind( &TimerWindow::callSelectCell, this ) );

    // Run the trace unit tests
    if ( hasTraceData() ) {
        callFun( std::bind( &TimerWindow::traceFun, this ) );
        if ( !traceWindow->runUnitTests() )
            return false;
        callFun( std::bind( &TimerWindow::closeTrace, this ) );
    }
    // Try to close the file
    callFun( std::bind( &TimerWindow::close, this ) );
    if ( !d_dataTimer.empty() ) {
        printf( "   Failed call to close\n" );
        return false;
    }
    return true;
}
