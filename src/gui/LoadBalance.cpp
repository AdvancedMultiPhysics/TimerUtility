#include "LoadBalance.h"
#include "ProfilerApp.h"
#include <qwt_plot.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_barchart.h>
#include <qwt_column_symbol.h>
#include <qwt_plot_layout.h>
#include <qwt_legend.h>
#include <qwt_scale_draw.h>


template<class TYPE>
inline QVector<double> toQVector( const std::vector<TYPE>& x )
{
    QVector<double> y(x.size());
    for (size_t i=0; i<x.size(); i++)
        y[i] = static_cast<double>(x[i]);
    return y;
}


LoadBalance::LoadBalance( QWidget *parent ):
    QwtPlot(parent), barChart(NULL)
{
    setAutoFillBackground( true );
    setPalette( QColor( "Linen" ) );

    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setLineWidth(2);
    canvas->setFrameStyle( QFrame::Box | QFrame::Sunken );
    canvas->setBorderRadius(10);

    QPalette canvasPalette( QColor(230,255,255) );
    canvasPalette.setColor( QPalette::Foreground, QColor(230,255,255) );
    canvas->setPalette( canvasPalette );

    setCanvas( canvas );

    // Create the plot
    barChart = new QwtPlotBarChart("Load Balance");
    barChart->setLegendMode( QwtPlotBarChart::LegendBarTitles );
    barChart->setLegendIconSize( QSize(0,0) );
    barChart->setLayoutPolicy( QwtPlotBarChart::AutoAdjustSamples );
    barChart->setLayoutHint( 4.0 ); // minimum width for a single bar
    barChart->setSpacing( 10 ); // spacing between bars
    barChart->setBaseline(1.0);
    QwtColumnSymbol *symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
    symbol->setLineWidth( 2 );
    symbol->setFrameStyle( QwtColumnSymbol::Raised );
    symbol->setPalette( QColor( 0, 50, 0 ) );
    barChart->setSymbol( symbol );
}


void LoadBalance::plot( const std::vector<float>& time )
{
    PROFILE_START("plot");
    N_procs = static_cast<int>(time.size());
    if ( time==data ) {
        replot();
        PROFILE_STOP2("plot");
        return;
    }
    data = time;

    // Add the data
    if ( time.size() < 100 ) {
        barChart->setSpacing( 10 ); // spacing between bars
    } else {
        barChart->setSpacing( 0 ); // spacing between bars
    }
    barChart->setSamples( toQVector(time) );
    barChart->attach( this );

    insertLegend( new QwtLegend() );

    float max_time = 0;
    for (size_t i=0; i<time.size(); i++)
        max_time = std::max(max_time,time[i]);
    /*QStringList rank_list;
    for (int i=0; i<N_procs; i++) {
        char tmp[10];
        sprintf(tmp,"%i",i+1);
        rank_list << tmp;
    }*/

    // Set the axis
    int xaxis = QwtPlot::xBottom;
    int yaxis = QwtPlot::yLeft;
    setAxisScale(xaxis,-1,N_procs+1);
    setAxisScale(yaxis,0,max_time);
    setAxisTitle(xaxis,"Rank");
    setAxisTitle(yaxis,"Time (s)");

    replot();
    PROFILE_STOP("plot");
}


void LoadBalance::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "distrowatch.pdf" );
}
