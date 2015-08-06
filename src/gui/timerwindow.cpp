#include <QtGui>
#include <QSqlDatabase>
#include <QSqlTableModel>

#include <memory>
#include <vector>
#include <set>

#include "timerwindow.h"
#include "ProfilerApp.h"


// stringf
inline std::string stringf( const char *format, ... )
{
    va_list ap; 
    va_start(ap,format); 
    char tmp[1024];
    vsprintf(tmp,format,ap);
    va_end(ap);
    return std::string(tmp);
}


// Simple matrix class
template<class TYPE>
class Matrix {
public:
    typedef std::shared_ptr<Matrix<TYPE>> shared_ptr;
    int N;
    int M;
    TYPE *data;
    Matrix(): N(0), M(0), data(NULL) {}
    Matrix(int N_, int M_): N(N_), M(M_), data(new TYPE[N_*M_]) { fill(0); }
    ~Matrix() { delete [] data; }
    TYPE& operator()(int i, int j) { return data[i+j*N]; }
    void fill( TYPE x ) { 
        for (int i=0; i<N*M; i++)
            data[i] = x;
    }
    TYPE min() const { 
        TYPE x = data[0];
        for (int i=0; i<N*M; i++)
            x = std::min(x,data[i]);
        return x;
    }
    TYPE max() const { 
        TYPE x = data[0];
        for (int i=0; i<N*M; i++)
            x = std::max(x,data[i]);
        return x;
    }
    TYPE sum() const { 
        TYPE x = 0;
        for (int i=0; i<N*M; i++)
            x += data[i];
        return x;
    }
    double mean() const { 
        TYPE x = sum();
        return static_cast<double>(x)/(N*M);
    }
protected:
    Matrix(const Matrix&);            // Private copy constructor
    Matrix& operator=(const Matrix&); // Private assignment operator
};


// Struct to hold the summary info for a timer
struct TimerSummary {
    id_struct id;                       //!<  Timer ID
    std::string message;                //!<  Timer message
    std::string file;                   //!<  Timer file
    int start;                          //!<  Timer start line (-1: never defined)
    int stop;                           //!<  Timer stop line (-1: never defined)
    std::vector<int> threads;           //!<  Threads that are active for this timer
    Matrix<int>::shared_ptr N;          //!<  Number of calls
    Matrix<double>::shared_ptr min;     //!<  Minimum time
    Matrix<double>::shared_ptr max;     //!<  Maximum time
    Matrix<double>::shared_ptr tot;     //!<  Total time
};


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


// Do we want to keep the trace
inline bool keepTrace( const TraceResults& trace, const std::vector<id_struct>& callList )
{
    bool keep = true;
    const id_struct* active = trace.active();
    for (size_t i=0; i<callList.size(); i++) {
        bool found = trace.id==callList[i];
        for (int j=0; j<trace.N_active; j++)
            found = found || callList[i]==active[j];
        keep = keep && found;
    }
    return keep;
}


/***********************************************************************
* Constructor/destructor                                               *
***********************************************************************/
TimerWindow::TimerWindow():
    timerTable(NULL), callLineText(NULL), back_button(NULL)
{

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
    back_button = new QPushButton("Back");
    connect(back_button, SIGNAL(released()), this, SLOT(backButton()));
    back_button->hide();

    // Create the layout
    QHBoxLayout *layout_callLine = new QHBoxLayout;
    layout_callLine->addWidget(callLineText);
    layout_callLine->addWidget(back_button);
    QWidget *widget_callLine = new QWidget;
    widget_callLine->setLayout(layout_callLine);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(widget_callLine);
    layout->addWidget(timerTable);
    setCentralWidget(new QWidget);
    centralWidget()->setLayout(layout);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    readSettings();

    setUnifiedTitleAndToolBarOnMac(true);

}
void TimerWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}
void TimerWindow::close()
{
    data = TimerMemoryResults();
    current_timers.clear();
    callList.clear();
    QWidget::setWindowTitle(QString("load_timer"));
    updateDisplay();
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
    current_timers.clear();
    callList.clear();
    updateDisplay();
}


/***********************************************************************
* Help                                                                 *
***********************************************************************/
void TimerWindow::about()
{
   QMessageBox::about(this, tr("About Application"), tr(
        "This is the graphical user interface for viewing"
        "the timer data from the ProfilerApp."));
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
    std::string filename = fileName.toStdString();
    if ( !filename.empty() ) {
        close();
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
            data = ProfilerApp::load( filename, -1, global );
            N_procs = data.N_procs;
            N_threads = 0;
            for (size_t i=0; i<data.timers.size(); i++) {
                for (size_t j=0; j<data.timers[i].trace.size(); j++)
                    N_threads = std::max<int>(N_threads,data.timers[i].trace[j].thread+1);
            }
            printf("%s loaded successfully\n",filename.c_str());
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
}


/***********************************************************************
* Update the display                                                   *
***********************************************************************/
void TimerWindow::updateDisplay()
{
    current_timers = getTimers();
    if ( data.timers.empty() ) {
        timerTable->hide();
        callLineText->hide();
        back_button->hide();
        return;
    }
    // Set the call line
    std::string call_stack;
    if ( !callList.empty() ) {
        const TimerResults& timer = findTimer(data.timers,callList[0]);
        call_stack = timer.message;
        for (size_t i=1; i<callList.size(); i++) {
            const TimerResults& timer = findTimer(data.timers,callList[i]);
            call_stack += " -> " + timer.message;
        }
    }
    callLineText->setText(call_stack.c_str());
    callLineText->show();
    if ( callList.empty() )
        back_button->hide();
    else
        back_button->show();
    // Fill the table data
    QStringList TableHeader;
    TableHeader << "Message" << "Filename" << "Thread" <<
        "Start Line" << "Stop Line" << "N calls" << "min time" <<
        "max time" << "total time" << "% time";
    timerTable->setRowCount(current_timers.size());
    timerTable->setColumnCount(10);
    timerTable->setColumnWidth(0,200);
    timerTable->setColumnWidth(1,200);
    timerTable->setColumnWidth(2, 80);
    timerTable->setColumnWidth(3, 80);
    timerTable->setColumnWidth(4, 80);
    timerTable->setColumnWidth(5, 80);
    timerTable->setColumnWidth(6, 95);
    timerTable->setColumnWidth(7, 95);
    timerTable->setColumnWidth(8, 95);
    timerTable->setColumnWidth(9, 85);
    timerTable->setHorizontalHeaderLabels(TableHeader);
    timerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (size_t i=0; i<current_timers.size(); i++) {
        timerTable->setRowHeight(i,25);
        const TimerSummary *timer = current_timers[i].get();
        QTableWidgetItem* message = new QTableWidgetItem(timer->message.c_str());
        QTableWidgetItem* file    = new QTableWidgetItem(timer->file.c_str());
        QTableWidgetItem* threads = new QTableWidgetItem(threadString(timer->threads).c_str());
        QTableWidgetItem* start   = new QTableWidgetItem(stringf("%i",timer->start).c_str());
        QTableWidgetItem* stop    = new QTableWidgetItem(stringf("%i",timer->stop).c_str());
        QTableWidgetItem* N       = new QTableWidgetItem(stringf("%i",(int)timer->N->mean()).c_str());
        QTableWidgetItem* min     = new QTableWidgetItem(stringf("%0.3e",timer->min->mean()).c_str());
        QTableWidgetItem* max     = new QTableWidgetItem(stringf("%0.3e",timer->max->mean()).c_str());
        QTableWidgetItem* tot     = new QTableWidgetItem(stringf("%0.3e",timer->tot->mean()).c_str());
        QTableWidgetItem* percent = new QTableWidgetItem("");
        message->setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        file->   setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        threads->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        start->  setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        stop->   setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        N->      setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        min->    setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        max->    setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        tot->    setTextAlignment( Qt::AlignLeft    | Qt::AlignVCenter );
        percent->setTextAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        timerTable->setItem(i,0,message);
        timerTable->setItem(i,1,file);
        timerTable->setItem(i,2,threads);
        timerTable->setItem(i,3,start);
        timerTable->setItem(i,4,stop);
        timerTable->setItem(i,5,N);
        timerTable->setItem(i,6,min);
        timerTable->setItem(i,7,max);
        timerTable->setItem(i,8,tot);
        timerTable->setItem(i,9,percent);
    }
    timerTable->show();
}


/***********************************************************************
* Get the timers of interest                                           *
***********************************************************************/
std::vector<std::shared_ptr<TimerSummary>> TimerWindow::getTimers()
{
    std::vector<std::shared_ptr<TimerSummary>> timers;
    timers.reserve(data.timers.size());
    // Get the timers of interest
    for (size_t i=0; i<data.timers.size(); i++) {
        std::shared_ptr<TimerSummary> timer(new TimerSummary);
        timer->id = data.timers[i].id;
        timer->message = data.timers[i].message;
        timer->file = data.timers[i].file;
        timer->start = data.timers[i].start;
        timer->stop = data.timers[i].stop;
        timer->N.reset( new Matrix<int>(N_threads,N_procs) );
        timer->min.reset( new Matrix<double>(N_threads,N_procs) );
        timer->max.reset( new Matrix<double>(N_threads,N_procs) );
        timer->tot.reset( new Matrix<double>(N_threads,N_procs) );
        timer->min->fill(1e100);
        std::set<int> threads;
        for (size_t j=0; j<data.timers[i].trace.size(); j++) {
            const TraceResults& trace = data.timers[i].trace[j];
            if ( !keepTrace(trace,callList) )
                continue;
            threads.insert(trace.thread);
            int& N = timer->N->operator()(trace.thread,trace.rank);
            double& min = timer->min->operator()(trace.thread,trace.rank);
            double& max = timer->max->operator()(trace.thread,trace.rank);
            double& tot = timer->tot->operator()(trace.thread,trace.rank);
            N += trace.N;
            min = std::min<double>(min,trace.min);
            max = std::max<double>(max,trace.max);
            tot += trace.tot;
        }
        timer->threads = std::vector<int>(threads.begin(),threads.end());
        if ( !threads.empty() )
            timers.push_back(timer);
    }
    return timers;
}


/***********************************************************************
* Select a cell in the table                                           *
***********************************************************************/
void TimerWindow::cellSelected( int row, int col )
{
    if ( col != 0 )
        return;
    id_struct id = current_timers[row]->id;
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
void TimerWindow::backButton( )
{
    if ( callList.empty() )
        return;
    callList.pop_back();
    updateDisplay();
}



void TimerWindow::documentWasModified()
{
    //setWindowModified(timerTable->document()->isModified());
}

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

    //saveAsAct = new QAction(tr("Save &As..."), this);
    //saveAsAct->setShortcuts(QKeySequence::SaveAs);
    //saveAsAct->setStatusTip(tr("Save the document under a new name"));
    //connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(exit()));

    //cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
    //cutAct->setShortcuts(QKeySequence::Cut);
    //cutAct->setStatusTip(tr("Cut the current selection's contents to the clipboard"));
    //connect(cutAct, SIGNAL(triggered()), timerTable, SLOT(cut()));

    //copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
    //copyAct->setShortcuts(QKeySequence::Copy);
    //copyAct->setStatusTip(tr("Copy the current selection's contents to the clipboard"));
    //connect(copyAct, SIGNAL(triggered()), timerTable, SLOT(copy()));

    //pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
    //pasteAct->setShortcuts(QKeySequence::Paste);
    //pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current selection"));
    //connect(pasteAct, SIGNAL(triggered()), timerTable, SLOT(paste()));

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    //cutAct->setEnabled(false);
    //copyAct->setEnabled(false);
    //connect(timerTable, SIGNAL(copyAvailable(bool)), cutAct, SLOT(setEnabled(bool)));
    //connect(timerTable, SIGNAL(copyAvailable(bool)), copyAct, SLOT(setEnabled(bool)));
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
}

void TimerWindow::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(closeAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(resetAct);
    //fileToolBar->addAction(saveAct);

/*QMenu *menu = new QMenu();
QAction *testAction = new QAction("test menu item", this);
menu->addAction(testAction);
QToolButton* toolButton = new QToolButton();
toolButton->setText("popup button");
toolButton->setMenu(menu);
toolButton->setPopupMode(QToolButton::InstantPopup);
fileToolBar->addWidget(toolButton);*/

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

