#ifndef LoadBalance_H
#define LoadBalance_H

#include <qwt_plot.h>
#include <qstringlist.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>


class LoadBalance: public QwtPlot
{
    Q_OBJECT

public:
    LoadBalance( QWidget * = NULL );
    virtual ~LoadBalance() {}
    void plot( const std::vector<float>& time );

public Q_SLOTS:
    void exportChart();

private:
    void populate();

    QwtPlotBarChart *barChart;
 	QwtPlotCurve *curvePlot[2];
    int N_procs;
    QVector<double> rank;
    QVector<double> time;

private:
    LoadBalance() {}
};

#endif
