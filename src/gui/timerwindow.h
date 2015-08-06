#ifndef TimerWindow_H
#define TimerWindow_H

#include <QMainWindow>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>

#include <memory>

#include "ProfilerApp.h"



class QAction;
class QMenu;
class QTableView;
class TimerSummary;


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
    void documentWasModified();
    void cellSelected(int nRow, int nCol);
    void backButton();

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
    std::vector<std::shared_ptr<TimerSummary>> getTimers();
    QString strippedName(const QString &fullFileName);

    QTableWidget *timerTable;
    QLineEdit *callLineText;
    QPushButton *back_button;

    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QAction *closeAct;
    QAction *openAct;
    QAction *resetAct;
    QAction *exitAct;
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
    QAction *aboutAct;

private:
    std::string lastPath;
    TimerMemoryResults data;
    std::vector<std::shared_ptr<TimerSummary>> current_timers;
    std::vector<id_struct> callList;
    int N_procs;
    int N_threads;
};

#endif
