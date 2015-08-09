#ifndef TimerWindow_H
#define TimerWindow_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
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

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void close();
    void exit();
    void open();
    void reset();
    void about();
    void savePerformance();
    void cellSelected(int nRow, int nCol);
    void backButtonPressed();
    void exclusiveFun();
    void subfunctionFun();
    void traceFun();
    void selectProcessor( int );

    void resizeEvent( QResizeEvent *e );
    void resizeDone();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void readSettings();
    void writeSettings();
    void loadFile(const QString &fileName);
    void setCurrentFile(const QString &fileName);
    void updateDisplay();
    std::vector<std::shared_ptr<TimerSummary>> getTimers() const;
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
    QAction *exclusiveAct;
    QAction *subfunctionsAct;

    QTimer resizeTimer;

protected:
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
};

#endif
