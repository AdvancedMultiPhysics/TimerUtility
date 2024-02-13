#ifndef TimerWindow_H
#define TimerWindow_H

#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include <memory>

#include "GuiTimerStructures.h"
#include "LoadBalance.h"
#include "ProfilerApp.h"
#include "ThreadedSlotsClass.h"


class QAction;
class QMenu;
class QTableView;
class TraceWindow;
class MemoryWindow;


class TimerWindow : public ThreadedSlotsClass
{
    Q_OBJECT

public:
    TimerWindow();
    TimerWindow( const TimerWindow & )            = delete;
    TimerWindow &operator=( const TimerWindow & ) = delete;
    virtual ~TimerWindow();

protected:
    void closeEvent( QCloseEvent *event );

public slots:
    void exit();
private slots:
    void close();
    void open();
    void reset();
    void about();
    void cellSelected( int nRow, int nCol );
    void backButtonPressed();
    void exclusiveFun();
    void subfunctionFun();
    void traceFun();
    void memoryFun();
    void selectProcessor( int );

    void resizeEvent( QResizeEvent *e );
    void resizeDone();

    // Developer functions
    void savePerformance();
    void runUnitTestsSlot();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    void loadFile( std::string filename, bool showFailure = true );
    void setCurrentFile( const QString &fileName );
    void updateDisplay();
    QString strippedName( const QString &fullFileName );

    QMenuBar *mainMenu;
    QTableWidget *timerTable;
    QLineEdit *callLineText;
    QPushButton *backButton;
    QToolButton *processorButton;
    QPushButton *exclusiveButton;
    QPushButton *subfunctionButton;
    QPushButton *traceButton;
    QPushButton *memoryButton;
    QAction *processorToolbar;
    QAction *exclusiveToolbar;
    QAction *subfunctionToolbar;
    QAction *traceToolbar;
    QAction *memoryToolbar;
    LoadBalance *loadBalance;

    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QAction *closeAct;
    QAction *openAct;
    QAction *resetAct;
    QAction *exitAct;
    QAction *aboutAct;
    QAction *savePerformanceTimers;
    QAction *runUnitTestAction;
    QAction *exclusiveAct;
    QAction *subfunctionsAct;
    QTimer resizeTimer;
    std::unique_ptr<QMenu> processorButtonMenu;

private:
    bool hasTraceData() const;

protected:
    std::vector<std::unique_ptr<TimerSummary>> getTimers() const;

    std::string lastPath;
    TimerMemoryResults d_data;
    std::vector<TimerSummary> d_dataTimer;
    std::vector<std::unique_ptr<TraceSummary>> d_dataTrace;
    std::vector<id_struct> d_callLine;
    std::vector<std::vector<uint64_t>> d_callStack;
    std::tuple<std::vector<uint64_t>, std::vector<std::vector<uint64_t>>> d_stackMap;
    int N_procs;
    int N_threads;
    int selected_rank;
    bool inclusiveTime;
    bool includeSubfunctions;
    mutable std::unique_ptr<TraceWindow> traceWindow;
    mutable std::unique_ptr<MemoryWindow> memoryWindow;

    friend class TraceWindow;
    friend class MemoryWindow;

    // Data for unit testing
public:
    bool runUnitTests( const std::string &file );

private:
    std::string unitTestFilename;
    void resetUnitTestRunning();
    void callLoadFile();
    void callSelectCell();
    void closeTrace();
};

#endif
