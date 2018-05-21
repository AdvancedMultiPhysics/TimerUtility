#ifndef MemoryPlot_H
#define MemoryPlot_H

#include <qstringlist.h>
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_curve.h>

#include "MemoryPlot.h"
#include "ProfilerApp.h"

#include <array>


class MemoryPlot : public QwtPlot
{
    Q_OBJECT

public:
    MemoryPlot( QWidget *parent, const std::vector<MemoryResults> &memory );
    virtual ~MemoryPlot();
    void plot( std::array<double, 2> t, int rank );
    void align( int left = -1, int right = -1 );

public Q_SLOTS:
    void exportChart();

private:
    void populate();
    std::array<size_t, 2> updateRankData( int rank );
    void plotRank( int rank, double scale );

    std::vector<QwtPlotCurve *> d_curvePlot;
    std::vector<std::vector<double>> d_time;
    std::vector<std::vector<double>> d_size;

    std::array<double, 2> d_t;
    int d_last_rank;

private:
    MemoryPlot() : d_memory( nullptr ), d_N_procs( 0 ) {}
    const std::vector<MemoryResults> *d_memory;
    const int d_N_procs;
};

#endif
