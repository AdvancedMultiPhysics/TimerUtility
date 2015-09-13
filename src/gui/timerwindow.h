#ifndef TimerWindow_H
#define TimerWindow_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QToolBar>
#include <QTimer>

#include <memory>

#include "ProfilerApp.h"
#include "LoadBalance.h"
#include "GuiTimerStructures.h"



class QAction;
class QMenu;
class QTableView;
class TimerSummary;
class TraceWindow;


class TimerWindow : public QMainWindow
{
    Q_OBJECT

public:
    TimerWindow();
    ~TimerWindow();

protected:
    void closeEvent(QCloseEvent *event);

public slots:
    void exit();
private slots:
    void close();
    void open();
    void reset();
    void about();
    void cellSelected(int nRow, int nCol);
    void backButtonPressed();
    void exclusiveFun();
    void subfunctionFun();
    void traceFun();
    void selectProcessor( int );

    void resizeEvent( QResizeEvent *e );
    void resizeDone();

    // Developer functions
    void savePerformance();
    void runUnitTestsSlot( );

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    void loadFile( std::string filename, bool showFailure=true );
    void setCurrentFile(const QString &fileName);
    void updateDisplay();
    QString strippedName(const QString &fullFileName);

    QTableWidget *timerTable;
    QLineEdit *callLineText;
    QPushButton *backButton;
    QToolButton *processorButton;
    QPushButton *exclusiveButton;
    QPushButton *subfunctionButton;
    QPushButton *traceButton;
    QAction *processorToolbar;
    QAction *exclusiveToolbar;
    QAction *subfunctionToolbar;
    QAction *traceToolbar;
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
    std::shared_ptr<QMenu> processorButtonMenu;

protected:
    std::vector<std::shared_ptr<TimerSummary>> getTimers() const;

    std::string lastPath;
    TimerMemoryResults d_data;
    std::vector<TimerSummary> d_dataTimer;
    std::vector<std::shared_ptr<TraceSummary>> d_dataTrace;
    std::vector<id_struct> callList;
    int N_procs;
    int N_threads;
    int selected_rank;
    bool inclusiveTime;
    bool includeSubfunctions;
    std::shared_ptr<TraceWindow> traceWindow;

friend class TraceWindow;

// Data for unit testing
public:
    int runUnitTests( const std::string& file );
private:
    std::string unitTestFilename;
    volatile bool unitTestRunning;
private slots:
    void resetUnitTestRunning( );
    void callLoadFile( );
};

#endif
