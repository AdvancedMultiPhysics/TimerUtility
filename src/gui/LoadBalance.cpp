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
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>


template<class TYPE>
inline QVector<double> toQVector( const std::vector<TYPE>& x )
{
    QVector<double> y(x.size());
    for (size_t i=0; i<x.size(); i++)
        y[i] = static_cast<double>(x[i]);
    return y;
}


inline QwtText qwtText( const QString& string, const QFont& font )
{
    QwtText text(string);
    text.setFont(font);
    return text;
}


LoadBalance::LoadBalance( QWidget *parent ):
    QwtPlot(parent), barChart(NULL)
{
    setTitle(qwtText("Load Balance",QFont("Times",12,QFont::Bold)));
    setAutoFillBackground( true );
    setPalette( QColor( "Linen" ) );
    setContentsMargins(0,0,0,0);
    for(int axis : {QwtPlot::xBottom,QwtPlot::xTop,QwtPlot::yLeft,QwtPlot::yRight}) {
        axisWidget(axis)->setMinBorderDist(0,0);
        axisWidget(axis)->setBorderDist(0,0);
        axisWidget(axis)->setMargin(0);
        axisWidget(axis)->setSpacing(0);
        plotLayout()->setCanvasMargin(axis,0);
    }

    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setLineWidth(0);
    canvas->setFrameStyle( QFrame::Box | QFrame::Sunken );
    canvas->setBorderRadius(0);
    QPalette canvasPalette( QColor(230,255,255) );
    canvasPalette.setColor( QPalette::Foreground, QColor(230,255,255) );
    canvas->setPalette( canvasPalette );
    canvas->setContentsMargins(0,0,0,0);
    setCanvas( canvas );

    // Create the barChart
    barChart = new QwtPlotBarChart("");
    barChart->setLegendMode( QwtPlotBarChart::LegendBarTitles );
    barChart->setLegendIconSize( QSize(0,0) );
    barChart->setLayoutPolicy( QwtPlotBarChart::AutoAdjustSamples );
    barChart->setLayoutHint( 4.0 ); // minimum width for a single bar
    barChart->setSpacing( 10 ); // spacing between bars
    barChart->setBaseline(0);
    QwtColumnSymbol *symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
    symbol->setLineWidth( 2 );
    symbol->setFrameStyle( QwtColumnSymbol::Raised );
    symbol->setPalette( QColor(0,50,0) );
    barChart->setSymbol( symbol );

    // Create the line plots
 	curvePlot[0] = new QwtPlotCurve("");
 	curvePlot[1] = new QwtPlotCurve("");
    curvePlot[0]->setLegendIconSize( QSize(0,0) );
    curvePlot[1]->setLegendIconSize( QSize(0,0) );

    // Plot some initial data to initialize the plot
    plot( std::vector<float>(10,1.0) );
    plot( std::vector<float>(1000,1.0) );
}


void LoadBalance::plot( const std::vector<float>& time_ )
{
    PROFILE_START("plot");
    N_procs = static_cast<int>(time_.size());
    QVector<double> new_time = toQVector(time_);
    if ( new_time==time ) {
        replot();
        PROFILE_STOP2("plot");
        return;
    }

    // Copy the data and compute the mean
    std::swap(time,new_time);
    rank.resize(N_procs);
    for (int i=0; i<N_procs; i++)
        rank[i] = i;
    double mean = 0;
    for (int i=0; i<N_procs; i++)
        mean += time[i];
    mean /= N_procs;

    // Add the data
    double range[2];
    if ( time.size() < 100 ) {
        if ( time.size() < 40 ) {
            barChart->setSpacing( 10 ); // spacing between bars
        } else {
            barChart->setSpacing( 0 ); // spacing between bars
        }
        barChart->setSamples( time );
        barChart->attach( this );
        barChart->setVisible(1);
        curvePlot[0]->attach( NULL );
        curvePlot[1]->setPen(QColor(204,0,0),3,Qt::DotLine);
        range[0] = -0.5;
        range[1] = N_procs-0.5;
    } else {
        barChart->attach( NULL );
        curvePlot[0]->setSamples( rank,  time  );
        curvePlot[0]->attach( this );
        curvePlot[0]->setVisible(1);
        curvePlot[0]->setPen(QColor(0,50,0),3,Qt::SolidLine);
        curvePlot[1]->setPen(QColor(204,0,0),2,Qt::DotLine);
        range[0] = 0;
        range[1] = N_procs-1;
    }
    QVector<double> rank2(2), mean2(2);
    rank2[0]=range[0];  rank2[1]=range[1];
    mean2[0]=mean;      mean2[1]=mean;
    curvePlot[1]->setSamples( rank2, mean2 );
    curvePlot[1]->attach( this );
    curvePlot[1]->setVisible(1);
    insertLegend( new QwtLegend() );

    // Create the rank ticks
    QwtScaleDiv xtick(range[0],range[1]);
    if ( rank.size() < 16 ) {
        xtick = QwtScaleDiv(range[0],range[1],QList<double>(),QList<double>(),rank.toList());
    } else {
        QList<double> ticks;
        for (int i=0; i<N_procs-1; i+=N_procs/16)
            ticks.push_back(i);
        ticks.push_back(N_procs-1);
        xtick = QwtScaleDiv(range[0],range[1],QList<double>(),QList<double>(),ticks);
    }

    // Set the axis
    double max_time = 0;
    for (int i=0; i<time.size(); i++)
        max_time = std::max(max_time,time[i]);
    int xaxis = QwtPlot::xBottom;
    int yaxis = QwtPlot::yLeft;
    setAxisScaleDiv(xaxis,xtick);
    setAxisScale(yaxis,0,1.1*max_time);
    setAxisTitle(xaxis,qwtText("Rank",QFont("Times",10,QFont::Bold)));
    setAxisTitle(yaxis,qwtText("Time (s)",QFont("Times",10,QFont::Bold)));
    setAxisScale(QwtPlot::xTop,0,0);

    replot();
    PROFILE_STOP("plot");
}


void LoadBalance::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "distrowatch.pdf" );
}
