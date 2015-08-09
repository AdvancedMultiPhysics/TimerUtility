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

#include "timerwindow.h"
#include "TableValue.h"
#include "ProfilerApp.h"

extern "C" {
    #include "assert.h"
}


// Function to convert the thread list to a string
std::string threadString( const std::vector<int>& x )
{
    if ( x.empty() )
        return std::string();
    if ( x.size() > 1 )
        return std::string("multiple");
    std::string s = stringf("%i",x[0]);
    for (size_t i=1; i<x.size(); i++)
        s += stringf("%s, %i",s.c_str(),x[i]);
    return s;
}


// Function to search and return the timer for an id
static const TimerResults& findTimer( const std::vector<TimerResults>& timers, id_struct id )
{
    for (size_t i=0; i<timers.size(); i++) {
        if ( timers[i].id == id )
            return timers[i];
    }
    printf("timer not found\n");
    static TimerResults null_timer;
    return null_timer;
}


// Get a list of active traces
inline std::vector<id_struct> getActive( const TraceResults& trace )
{
    std::vector<id_struct> active(trace.N_active);
    for (size_t i=0; i<active.size(); i++)
        active[i] = trace.active()[i];
    return active;
}


// Do we want to keep the trace
inline bool keepTrace( const TraceSummary& trace, 
    const std::vector<id_struct>& callList, const bool keep_subfunctions )
{
    bool keep = true;
    for (size_t i=0; i<callList.size(); i++) {
        bool found = trace.id==callList[i];
        for (size_t j=0; j<trace.active.size(); j++)
            found = found || callList[i]==trace.active[j];
        keep = keep && found;
    }
    if ( keep && !keep_subfunctions ) {
        keep = keep && trace.active.size()<=callList.size();
        if ( trace.active.size()==callList.size() ) {
            // We should not find id in the call list
            bool found = false;
            for (size_t i=0; i<callList.size(); i++)
                found = found || trace.id==callList[i];
            keep = keep && !found;
        }
    }
    return keep;
}


// Compute the min of a vector
template<class TYPE> TYPE min( const std::vector<TYPE>& x )
{
    TYPE y = std::numeric_limits<TYPE>::max();
    for (size_t i=0; i<x.size(); i++)
        y = std::min(y,x[i]);
    return y;
}


// Compute the max of a vector
template<class TYPE> TYPE max( const std::vector<TYPE>& x )
{
    TYPE y = std::numeric_limits<TYPE>::min();
    for (size_t i=0; i<x.size(); i++)
        y = std::max(y,x[i]);
    return y;
}


// Compute the min of a vector
template<class TYPE> TYPE sum( const std::vector<TYPE>& x )
{
    TYPE y = 0;
    for (size_t i=0; i<x.size(); i++)
        y += x[i];
    return y;
}


// Compute the mean of a vector
template<class TYPE> double mean( const std::vector<TYPE>& x )
{
    return static_cast<double>(sum(x))/static_cast<double>(x.size());
}


// Function to add an action (with connection) to a menu
#define ADD_MENU_ACTION( menu, string, arg ) \
    do {                                            \
        QAction* action = new QAction(string,this); \
        connect(action, SIGNAL(triggered()), signalMapper, SLOT(map())); \
        signalMapper->setMapping(action,arg);       \
        menu->addAction( action );                  \
    } while(0)                                      \


inline void ERROR_MSG( const std::string& msg ) {
    throw std::logic_error(msg);
}


#define ASSERT(EXP) do {                                            \
    if ( !(EXP) ) {                                                 \
        std::stringstream stream;                                   \
        stream << "Failed assertion: " << #EXP                      \
            << " " << __FILE__ << " " << __LINE__;                  \
        ERROR_MSG(stream.str());                                    \
    }                                                               \
}while(0)


/***********************************************************************
* Constructor/destructor                                               *
***********************************************************************/
TimerWindow::TimerWindow():
    timerTable(NULL), callLineText(NULL), backButton(NULL), processorButton(NULL)
{
    PROFILE_START("TimerWindow constructor");
    QWidget::setWindowTitle(QString("load_timer"));

    // Create the timer table
    timerTable = new QTableWidget;
    timerTable->hide();
    connect( timerTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(cellSelected(int,int) ) );

    // Create the call line
    callLineText = new QLineEdit(QString(""));
    callLineText->setFrame(false);
    callLineText->setReadOnly(true);
    QString bgColorName = palette().color(QPalette::Normal, QPalette::Window).name();
    QString strStyleSheet = QString("QLineEdit {background-color: ").append(bgColorName).append("}");
    callLineText->setStyleSheet(strStyleSheet);

    // Back button
    backButton = new QPushButton("Back");
    connect(backButton, SIGNAL(released()), this, SLOT(backButtonPressed()));
    backButton->hide();

    // Create the load balance chart
    loadBalance = new LoadBalance( this );
    loadBalance->hide();

    // Create the layout
    QHBoxLayout *layout_callLine = new QHBoxLayout;
    layout_callLine->addWidget(callLineText);
    layout_callLine->addWidget(backButton);
    QWidget *widget_callLine = new QWidget;
    widget_callLine->setLayout(layout_callLine);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setContentsMargins(QMargins(0,0,0,0));
    layout->setSpacing(0);
    layout->addWidget(widget_callLine);
    layout->addWidget(timerTable);
    layout->addWidget(loadBalance);
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    // Create resize event
    resizeTimer.setSingleShot( true );
    connect( &resizeTimer, SIGNAL(timeout()), SLOT(resizeDone()) );

    setUnifiedTitleAndToolBarOnMac(true);
    reset();
    updateDisplay();
    PROFILE_STOP("TimerWindow constructor");
}
void TimerWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}
void TimerWindow::close()
{
    d_data = TimerMemoryResults();
    d_dataTimer.clear();
    d_dataTrace.clear();
    QWidget::setWindowTitle(QString("load_timer"));
    reset();
}
void TimerWindow::exit()
{
    qApp->quit();
}


/***********************************************************************
* Reset the view                                                       *
***********************************************************************/
void TimerWindow::reset()
{
    callList.clear();
    inclusiveTime = true;
    includeSubfunctions = true;
    updateDisplay();
}


/***********************************************************************
* Help                                                                 *
***********************************************************************/
void TimerWindow::about()
{
   QMessageBox::about(this, tr("About Application"), tr(
        "This is the graphical user interface for viewing "
        "the timer data from the ProfilerApp."));
}


/***********************************************************************
* Save the timers for the application                                  *
***********************************************************************/
void TimerWindow::savePerformance()
{
    std::string filename = QFileDialog::getSaveFileName( this, 
        "Name of file to save", lastPath.c_str(), "*.0.timer *.1.timer" ).toStdString();
    if ( !filename.empty() ) {
        size_t i = filename.rfind('.',filename.rfind(".timer")-1);
        filename = filename.substr(0,i);
        PROFILE_SAVE(filename);
    }
}


/***********************************************************************
* Load timer data                                                      *
***********************************************************************/
void TimerWindow::open()
{
    QString filename = QFileDialog::getOpenFileName( this, 
        "Select the timer file", lastPath.c_str(), "*.0.timer *.1.timer" );
    loadFile(filename);
}
void TimerWindow::loadFile(const QString &fileName)
{
    PROFILE_START("loadFile");
    std::string filename = fileName.toStdString();
    if ( !filename.empty() ) {
        // Clear existing data
        close();
        // Get the filename and path
        bool global = filename.rfind(".0.timer")!=std::string::npos;
        size_t i = filename.rfind('.',filename.rfind(".timer")-1);
        filename = filename.substr(0,i);
        int pos = -1;
        if ( filename.rfind((char)47) != std::string::npos )
            pos = std::max<int>(pos,filename.rfind((char)47));
        if ( filename.rfind((char)92) != std::string::npos )
            pos = std::max<int>(pos,filename.rfind((char)92));
        std::string path, file;
        if ( pos != -1 ) {
            path = filename.substr(0,pos);
            file = filename.substr(pos+1);
            lastPath = path;
        }
        std::string title = file + " (" + path + ")";
        QWidget::setWindowTitle(title.c_str());
        try {
            // Load the timer data
            PROFILE_START("ProfilerApp::load");
            d_data = ProfilerApp::load( filename, -1, global );
            PROFILE_STOP("ProfilerApp::load");
            N_procs = d_data.N_procs;
            N_threads = 0;
            selected_rank = -1;
            for (size_t i=0; i<d_data.timers.size(); i++) {
                for (size_t j=0; j<d_data.timers[i].trace.size(); j++)
                    N_threads = std::max<int>(N_threads,d_data.timers[i].trace[j].thread+1);
            }
            // Get the global summary data
            d_dataTimer.clear();
            d_dataTimer.resize(d_data.timers.size());
            d_dataTrace.clear();
            d_dataTrace.reserve(1000);
            for (size_t i=0; i<d_data.timers.size(); i++) {
                TimerSummary& timer = d_dataTimer[i];
                timer.id = d_data.timers[i].id;
                timer.message = d_data.timers[i].message;
                timer.file = d_data.timers[i].file;
                timer.start = d_data.timers[i].start;
                timer.stop = d_data.timers[i].stop;
                timer.threads.clear();
                timer.N.resize(N_procs,0);
                timer.min.resize(N_procs,1e30);
                timer.max.resize(N_procs,0);
                timer.tot.resize(N_procs,0);
                timer.trace.clear();
                for (size_t j=0; j<d_data.timers[i].trace.size(); j++) {
                    int index = -1;
                    std::vector<id_struct> active = getActive(d_data.timers[i].trace[j]);
                    for (size_t k=0; k<timer.trace.size(); k++) {
                        if ( timer.trace[k]->active == active ) {
                            index = static_cast<int>(k);
                            break;
                        }
                    }
                    if ( index == -1 ) {
                        index = static_cast<int>(timer.trace.size());
                        size_t k = d_dataTrace.size();
                        d_dataTrace.push_back( std::shared_ptr<TraceSummary>(new TraceSummary()) );
                        d_dataTrace[k]->id = d_data.timers[i].trace[j].id;
                        d_dataTrace[k]->active = active;
                        d_dataTrace[k]->N.resize(N_procs,0);
                        d_dataTrace[k]->min.resize(N_procs,1e30);
                        d_dataTrace[k]->max.resize(N_procs,0);
                        d_dataTrace[k]->tot.resize(N_procs,0);
                        timer.trace.push_back(d_dataTrace[k].get());
                    }
                    TraceSummary *trace = const_cast<TraceSummary*>(timer.trace[index]);
                    int rank = d_data.timers[i].trace[j].rank;
                    trace->threads.insert(d_data.timers[i].trace[j].thread);
                    trace->N[rank] += d_data.timers[i].trace[j].N;
                    trace->min[rank] = std::min(trace->min[rank],d_data.timers[i].trace[j].min);
                    trace->max[rank] = std::max(trace->max[rank],d_data.timers[i].trace[j].max);
                    trace->tot[rank] += d_data.timers[i].trace[j].tot;
                }
                std::set<int> ids;
                for (size_t j=0; j<timer.trace.size(); j++) {
                    const TraceSummary *trace = timer.trace[j];
                    for (int k=0; k<N_procs; k++) {
                        timer.N[k] += trace->N[k];
                        timer.min[k] = std::min(timer.min[k],trace->min[k]);
                        timer.max[k] = std::max(timer.max[k],trace->max[k]);
                        timer.tot[k] += trace->tot[k];
                    }
                    ids.insert(trace->threads.begin(),trace->threads.end());
                }
                timer.threads = std::vector<int>(ids.begin(),ids.end());
            }
            // Update the display
            printf("%s loaded successfully\n",filename.c_str());
            if ( N_procs > 1 ) {
                QMenu *menu = new QMenu();
                QSignalMapper* signalMapper = new QSignalMapper(this);
                ADD_MENU_ACTION(menu,"Average",-1);
                ADD_MENU_ACTION(menu,"Minimum",-2);
                ADD_MENU_ACTION(menu,"Maximum",-3);
                for (int i=0; i<N_procs; i++)
                    ADD_MENU_ACTION(menu,stringf("Rank %i",i).c_str(),i);
                connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(selectProcessor(int)));
                processorButton->setText("Average");
                processorButton->setMenu(menu);
            }
            updateDisplay();
        } catch ( std::exception& e ) {
            std::string msg = e.what();
            QMessageBox::information( this, 
                tr("Error loading file"), 
                tr(msg.c_str()) );
        } catch (...) {
            QMessageBox::information( this, 
                tr("Error loading file"), 
                tr("Caught unknown exception") );
        }
    }
    PROFILE_STOP("loadFile");
}


/***********************************************************************
* Update the display                                                   *
***********************************************************************/
template<class TYPE>
inline TYPE getTableData( const std::vector<TYPE>& x, int rank )
{
    TYPE y;
    if ( rank==-1 ) {
        y = static_cast<TYPE>(mean(x));
    } else if ( rank==-2 ) {
        y = static_cast<TYPE>(min(x));
    } else if ( rank==-3 ) {
        y = static_cast<TYPE>(max(x));
    } else {
        y = x[rank];
    }
    return y;
}
void TimerWindow::updateDisplay()
{
    PROFILE_START("updateDisplay");
    std::vector<std::shared_ptr<TimerSummary>> current_timers = getTimers();
    if ( d_data.timers.empty() ) {
        timerTable->hide();
        callLineText->hide();
        backButton->hide();
        loadBalance->hide();
        processorToolbar->setVisible(false);
        exclusiveToolbar->setVisible(false);
        subfunctionToolbar->setVisible(false);
        PROFILE_STOP2("updateDisplay");
        return;
    }
    if ( N_procs > 1 )
        processorToolbar->setVisible(true);
    exclusiveToolbar->setVisible(true);
    subfunctionToolbar->setVisible(true);
    if ( includeSubfunctions ) {
        subfunctionButton->setText("Hide Subfunctions");
        subfunctionButton->setFlat(false);
    } else {
        subfunctionButton->setText("Show Subfunctions");
        subfunctionButton->setFlat(true);
    }
    if ( inclusiveTime ) {
        exclusiveButton->setText("Exclusive Time");
        exclusiveButton->setFlat(false);
    } else {
        exclusiveButton->setText("Inclusive Time");
        exclusiveButton->setFlat(true);
    }
    // Set the call line
    std::string call_stack;
    if ( !callList.empty() ) {
        const TimerResults& timer = findTimer(d_data.timers,callList[0]);
        call_stack = timer.message;
        for (size_t i=1; i<callList.size(); i++) {
            const TimerResults& timer = findTimer(d_data.timers,callList[i]);
            call_stack += " -> " + timer.message;
        }
        callLineText->setText(call_stack.c_str());
        callLineText->show();
        backButton->show();
    } else {
        callLineText->setText("");
        callLineText->hide();
        backButton->hide();
    }
    // Fill the table data
    QStringList TableHeader;
    TableHeader << "id" << "Message" << "Filename" << "Thread" <<
        "Start Line" << "Stop Line" << "N calls" << "min time" <<
        "max time" << "total time" << "% time";
    timerTable->clear();
    timerTable->setRowCount( 0);
    timerTable->setRowCount(current_timers.size());
    timerTable->setColumnCount(11);
    timerTable->QTableView::setColumnHidden(0,true);
    timerTable->setColumnWidth(1,200);
    timerTable->setColumnWidth(2,200);
    timerTable->setColumnWidth(3, 80);
    timerTable->setColumnWidth(4, 80);
    timerTable->setColumnWidth(5, 80);
    timerTable->setColumnWidth(6, 80);
    timerTable->setColumnWidth(7, 95);
    timerTable->setColumnWidth(8, 95);
    timerTable->setColumnWidth(9, 95);
    timerTable->setColumnWidth(10,85);
    timerTable->setHorizontalHeaderLabels(TableHeader);
    timerTable->verticalHeader()->setVisible(false);
    timerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    size_t N_timers = current_timers.size();
    std::vector<int> N_data(N_timers);
    std::vector<double> min_data(N_timers), max_data(N_timers), tot_data(N_timers);
    for (size_t i=0; i<N_timers; i++) {
        N_data[i] = getTableData(current_timers[i]->N,selected_rank);
        min_data[i] = getTableData(current_timers[i]->min,selected_rank);
        max_data[i] = getTableData(current_timers[i]->max,selected_rank);
        tot_data[i] = getTableData(current_timers[i]->tot,selected_rank);
    }
    double tot_max = max(tot_data);
    std::vector<double> ratio(N_timers,0);
    for (size_t i=0; i<N_timers; i++)
        ratio[i] = tot_data[i]/tot_max;
    for (size_t i=0; i<N_timers; i++) {
        timerTable->setRowHeight(i,25);
        const TimerSummary *timer = current_timers[i].get();
        QTableWidgetItem* id      = new QTableWidgetItem(timer->id.string().c_str());
        QTableWidgetItem* message = new QTableWidgetItem(timer->message.c_str());
        QTableWidgetItem* file    = new QTableWidgetItem(timer->file.c_str());
        QTableWidgetItem* threads = new QTableWidgetItem(threadString(timer->threads).c_str());
        QTableWidgetItem* start   = new TableValue(timer->start,"%i");
        QTableWidgetItem* stop    = new TableValue(timer->stop,"%i");
        QTableWidgetItem* N       = new TableValue(N_data[i],"%i");
        QTableWidgetItem* min     = new TableValue(min_data[i],"%0.3e");
        QTableWidgetItem* max     = new TableValue(max_data[i],"%0.3e");
        QTableWidgetItem* total   = new TableValue(tot_data[i],"%0.3e");
        QTableWidgetItem* percent = new TableValue(100*ratio[i],"%0.2f");
        id     ->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        message->setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        file->   setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        threads->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        start->  setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        stop->   setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        N->      setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        min->    setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        max->    setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        total->  setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        percent->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        timerTable->setItem(i,0,id);
        timerTable->setItem(i,1,message);
        timerTable->setItem(i,2,file);
        timerTable->setItem(i,3,threads);
        timerTable->setItem(i,4,start);
        timerTable->setItem(i,5,stop);
        timerTable->setItem(i,6,N);
        timerTable->setItem(i,7,min);
        timerTable->setItem(i,8,max);
        timerTable->setItem(i,9,total);
        timerTable->setItem(i,10,percent);
    }
    timerTable->setSortingEnabled(true);
    timerTable->sortItems(10,Qt::DescendingOrder);
    timerTable->show();
    // Update the load balance plot
    if ( N_procs>1 && !current_timers.empty() ) {
        // Plot the timer with the largest time
        size_t index = 0;
        double value = 0;
        for (size_t i=0; i<N_timers; i++) {
            if ( tot_data[i] > value ) {
                value = tot_data[i];
                index = i;
            }
        }
        loadBalance->plot(current_timers[index]->tot);
        loadBalance->setFixedHeight(centralWidget()->height()/4);
        loadBalance->show();
    } else {
        loadBalance->hide();
    }
    PROFILE_STOP("updateDisplay");
}


/***********************************************************************
* Get the timers of interest                                           *
***********************************************************************/
std::vector<std::shared_ptr<TimerSummary>> TimerWindow::getTimers() const
{
    PROFILE_START("getTimers");
    std::vector<std::shared_ptr<TimerSummary>> timers;
    timers.reserve(d_data.timers.size());
    // Get the timers of interest
    for (size_t i=0; i<d_dataTimer.size(); i++) {
        std::shared_ptr<TimerSummary> timer(new TimerSummary());
        timer->id = d_dataTimer[i].id;
        timer->message = d_dataTimer[i].message;
        timer->file = d_dataTimer[i].file;
        timer->start = d_dataTimer[i].start;
        timer->stop = d_dataTimer[i].stop;
        if ( callList.empty() ) {
            // Special case where we want all timers/trace results
            timer->trace = d_dataTimer[i].trace;
        } else {
            // Get the timers that have all of the call hierarchy
            timer->trace.resize(0);
            timer->trace.reserve(d_dataTimer[i].trace.size());
            for (size_t j=0; j<d_dataTimer[i].trace.size(); j++) {
                const TraceSummary *trace = d_dataTimer[i].trace[j];
                ASSERT(trace!=NULL);
                if ( keepTrace(*trace,callList,true) )
                    timer->trace.push_back(d_dataTimer[i].trace[j]);
            }
        }
        if ( !timer->trace.empty() )
            timers.push_back(timer);
    }
    // Keep only the traces that are directly inherited from the current call hierarchy
    if ( !includeSubfunctions ) {
        std::map<id_struct,int> id_map;
        for (size_t i=0; i<d_dataTimer.size(); i++)
            id_map.insert(std::pair<id_struct,int>(d_dataTimer[i].id,i));
        std::vector<id_struct> ids(timers.size());
        for (size_t i=0; i<ids.size(); i++)
            ids[i] = timers[i]->id;
        std::vector<bool> keep(timers.size(),false);
        for (size_t i=0; i<ids.size(); i++) {
            TimerSummary *timer = timers[i].get();
            std::vector<const TraceSummary*> trace2 = timer->trace;
            timer->trace.resize(0);
            for (size_t j=0; j<trace2.size(); j++) {
                const std::vector<id_struct>& id2 = trace2[j]->active;
                std::vector<int> tmp(id_map.size(),0);
                for (size_t k=0; k<ids.size(); k++)
                    tmp[id_map[ids[k]]]++;
                for (size_t k=0; k<id2.size(); k++)
                    tmp[id_map[id2[k]]]++;
                int tmp2 = 0;
                for (size_t k=0; k<tmp.size(); k++) {
                    if ( tmp[k]==2)
                        tmp2++;
                }
                if ( tmp2<=1 ) {
                    keep[i] = true;
                    timer->trace.push_back(trace2[j]);
                }
            }
        }
        size_t k=0;
        for (size_t i=0; i<ids.size(); i++) {
            if ( keep[i] ) {
                timers[k] = timers[i];
                k++;
            }
        }
        timers.resize(k);
    }
    // Compute the timer statistics
    for (size_t i=0; i<timers.size(); i++) {
        TimerSummary *timer = timers[i].get();
        timer->N.resize(N_procs,0);
        timer->min.resize(N_procs,1e100);
        timer->max.resize(N_procs,0.0);
        timer->tot.resize(N_procs,0.0);
        std::set<int> threads;
        for (size_t j=0; j<timer->trace.size(); j++) {
            const TraceSummary *trace = timer->trace[j];
            threads.insert(trace->threads.begin(),trace->threads.end());
            for (int k=0; k<N_procs; k++) {
                timer->N[k] += trace->N[k];
                timer->min[k] = std::min<double>(timer->min[k],trace->min[k]);
                timer->max[k] = std::max<double>(timer->max[k],trace->max[k]);
                timer->tot[k] += trace->tot[k];
            }
        }
        timer->threads = std::vector<int>(threads.begin(),threads.end());
    }
    // Update the timers to remove the time from sub-timers if necessary
    if ( !inclusiveTime ) {
        std::vector<std::vector<int>> tmp(timers.size());
        for (size_t i=0; i<timers.size(); i++) {
            tmp[i].resize(timers[i]->trace.size());
            for (size_t j=0; j<timers[i]->trace.size(); j++)
                tmp[i][j] = timers[i]->trace[j]->active.size();
        }
        for (size_t i1=0; i1<timers.size(); i1++) {
            for (size_t j1=0; j1<timers[i1]->trace.size(); j1++) {
                std::vector<id_struct> a1 = timers[i1]->trace[j1]->active;
                a1.push_back(timers[i1]->id);
                std::sort(a1.begin(),a1.end());
                for (size_t i2=0; i2<tmp.size(); i2++) {
                    for (size_t j2=0; j2<tmp[i2].size(); j2++) {
                        if ( tmp[i2][j2] == (int)a1.size() ) {
                            const std::vector<id_struct>& a2 = timers[i2]->trace[j2]->active;
                            if ( a1==a2 ) {
                                const std::vector<float>& tot = timers[i2]->trace[j2]->tot;
                                for (int k=0; k<N_procs; k++)
                                    timers[i1]->tot[k] -= tot[k];
                            }
                        }
                    }
                }
            }
        }
    }
    PROFILE_STOP("getTimers");
    return timers;
}


/***********************************************************************
* Select a cell in the table                                           *
***********************************************************************/
void TimerWindow::cellSelected( int row, int col )
{
    if ( col != 1 )
        return;
    id_struct id(timerTable->item(row,0)->text().toStdString());
    bool found = false;
    for (size_t i=0; i<callList.size(); i++)
        found = found || callList[i]==id;
    if ( !found )
        callList.push_back( id );
    updateDisplay();
}


/***********************************************************************
* Back button                                                          *
***********************************************************************/
void TimerWindow::backButtonPressed( )
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
    openAct = new QAction(QIcon(":/images/open.png"), tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    closeAct = new QAction(QIcon(":/images/new.png"), tr("&Close"), this);
    closeAct->setShortcuts(QKeySequence::Close);
    closeAct->setStatusTip(tr("Create a new file"));
    connect(closeAct, SIGNAL(triggered()), this, SLOT(close()));

    resetAct = new QAction(QIcon(":/images/refresh.png"), tr("&Reset"), this);
    resetAct->setShortcuts(QKeySequence::Refresh);
    resetAct->setStatusTip(tr("Reset the view"));
    connect(resetAct, SIGNAL(triggered()), this, SLOT(reset()));

    exclusiveAct = new QAction(tr("&Exclusive Time"), this);
    exclusiveAct->setStatusTip(tr("Exclusive Time"));
    connect(exclusiveAct, SIGNAL(triggered()), this, SLOT(exclusiveFun()));

    subfunctionsAct = new QAction(tr("&Hide Subfunctions"), this);
    subfunctionsAct->setStatusTip(tr("Hide Subfunctions"));
    connect(subfunctionsAct, SIGNAL(triggered()), this, SLOT(subfunctionFun()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(exit()));

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    savePerformanceTimers = new QAction(tr("&Save performance"), this);
    savePerformanceTimers->setStatusTip(tr("Save performance timers"));
    connect(savePerformanceTimers, SIGNAL(triggered()), this, SLOT(savePerformance()));
    
}

void TimerWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(closeAct);
    fileMenu->addAction(resetAct);
    //fileMenu->addAction(saveAct);
    //fileMenu->addAction(saveAsAct);
    //fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    //editMenu = menuBar()->addMenu(tr("&Edit"));
    //editMenu->addAction(cutAct);
    //editMenu->addAction(copyAct);
    //editMenu->addAction(pasteAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);

    // Create developer menu on rhs
    QMenuBar *bar = new QMenuBar(this);
    QMenu *developer = bar->addMenu(tr("&Developer"));
    developer->addAction(savePerformanceTimers);
    menuBar()->setCornerWidget(bar);
}

void TimerWindow::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(closeAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addSeparator();
    fileToolBar->addAction(resetAct);
    // Processor popup
    processorButton = new QToolButton();
    processorButton->setPopupMode(QToolButton::InstantPopup);
    processorToolbar = fileToolBar->addWidget(processorButton);
    // Exclusive time
    exclusiveButton = new QPushButton("Exclusive Time");
    connect(exclusiveButton, SIGNAL(released()), this, SLOT(exclusiveFun()));
    exclusiveToolbar = fileToolBar->addWidget(exclusiveButton);
    // Hide subfunctions
    subfunctionButton = new QPushButton("Hide Subfunctions");
    connect(subfunctionButton, SIGNAL(released()), this, SLOT(subfunctionFun()));
    subfunctionToolbar = fileToolBar->addWidget(subfunctionButton);

    //editToolBar = addToolBar(tr("Edit"));
    //editToolBar->addAction(cutAct);
    //editToolBar->addAction(copyAct);
    //editToolBar->addAction(pasteAct);
}

void TimerWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void TimerWindow::readSettings()
{
    QSettings settings("Trolltech", "Application Example");
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    resize(size);
    move(pos);
}

void TimerWindow::writeSettings()
{
    QSettings settings("Trolltech", "Application Example");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
}


/***********************************************************************
* Reszie functions                                                     *
***********************************************************************/
void TimerWindow::resizeEvent( QResizeEvent *e )
{
    resizeTimer.start( 500 );
    QMainWindow::resizeEvent(e);
}
void TimerWindow::resizeDone()
{
    loadBalance->setFixedHeight(centralWidget()->height()/4);
}

