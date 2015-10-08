#ifndef LoadBalance_H
#define LoadBalance_H

#include <memory>

#include <qwt_plot.h>
#include <qstringlist.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_canvas.h>

#include <QMouseEvent>

class DrawBoxClass;


class LoadBalance: public QwtPlot
{
    Q_OBJECT

public:
    LoadBalance( QWidget * = NULL );
    virtual ~LoadBalance();
    void plot( const std::vector<float>& time );

public Q_SLOTS:
    void exportChart();

private:
    void populate();

    QwtPlotCanvas *canvas;
    QwtPlotBarChart *barChart;
 	QwtPlotCurve *curvePlot[2];
    int N_procs;
    QVector<double> rank;
    QVector<double> time;
    std::array<double,4> global_range;
    std::array<double,4> plot_range;
    void zoom( const std::array<double,4>& range );

private:
    LoadBalance() {}

private:
    bool active;
    QPoint start, last;
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    std::shared_ptr<DrawBoxClass> zoom_box;

};

#endif
