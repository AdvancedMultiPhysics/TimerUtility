#ifndef LoadBalance_H
#define LoadBalance_H

#include <qwt_plot.h>
#include <qstringlist.h>
#include <qwt_plot_barchart.h>


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
    int N_procs;
    std::vector<float> data;

private:
    LoadBalance() {}
};

#endif
