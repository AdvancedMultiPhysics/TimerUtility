#include "LoadBalance.h"
#include "ProfilerApp.h"
#include "UtilityMacros.h"

#include <QPainter>

#include <cmath>

DISABLE_WARNINGS
#include <qwt_column_symbol.h>
#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_renderer.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_scale_widget.h>
ENABLE_WARNINGS


// Class to draw a box
class DrawBoxClass : public QWidget
{
public:
    DrawBoxClass( QWidget* parent, QRect pos_, int boarder = 0 )
        : QWidget( parent ), d_boarder( boarder ), pos( pos_ )
    {
        move( pos );
    }
    // Move to the given position
    void move( QRect pos_ )
    {
        pos               = pos_;
        const QRect& geom = parentWidget()->geometry();
        setGeometry( QRect( 0, 0, geom.width(), geom.height() ) );
        this->update();
    }
    // Update
    void redraw() { move( pos ); }
    // Draw the rectangle
    void paintEvent( QPaintEvent* ) override
    {
        QPainter p( this );
        QPen pen( QColor( 60, 0, 0 ) );
        pen.setWidth( 3 );
        p.setPen( pen );
        p.setRenderHint( QPainter::Antialiasing );
        p.drawRect( pos );
    }

protected:
    int d_boarder;
    QRect pos;
};


template<class TYPE>
inline QVector<double> toQVector( const std::vector<TYPE>& x )
{
    QVector<double> y( x.size() );
    for ( size_t i = 0; i < x.size(); i++ )
        y[i] = static_cast<double>( x[i] );
    return y;
}


inline QwtText qwtText( const QString& string, const QFont& font )
{
    QwtText text( string );
    text.setFont( font );
    return text;
}


LoadBalance::LoadBalance( QWidget* parent )
    : QwtPlot( parent ), canvas( nullptr ), barChart( nullptr ), active( false )
{
    setTitle( qwtText( "Load Balance", QFont( "Times", 12, QFont::Bold ) ) );
    setAutoFillBackground( true );
    setPalette( QColor( "Linen" ) );
    setContentsMargins( 0, 0, 0, 0 );
    for ( int axis : { QwtPlot::xBottom, QwtPlot::xTop, QwtPlot::yLeft, QwtPlot::yRight } ) {
        axisWidget( axis )->setMinBorderDist( 0, 0 );
        axisWidget( axis )->setBorderDist( 0, 0 );
        axisWidget( axis )->setMargin( 0 );
        axisWidget( axis )->setSpacing( 0 );
        plotLayout()->setCanvasMargin( axis, 0 );
    }

    canvas = new QwtPlotCanvas();
    canvas->setLineWidth( 0 );
    canvas->setFrameStyle( QFrame::Box | QFrame::Sunken );
    canvas->setBorderRadius( 0 );
    QPalette canvasPalette( QColor( 230, 255, 255 ) );
    canvasPalette.setColor( QPalette::Foreground, QColor( 230, 255, 255 ) );
    canvas->setPalette( canvasPalette );
    canvas->setContentsMargins( 0, 0, 0, 0 );
    setCanvas( canvas );
    zoom_box.reset( new DrawBoxClass( canvas, QRect( 0, 0, 0, 0 ), 2 ) );
    zoom_box->hide();

    // Create the barChart
    barChart = new QwtPlotBarChart( "" );
    barChart->setLegendMode( QwtPlotBarChart::LegendBarTitles );
    barChart->setLegendIconSize( QSize( 0, 0 ) );
    barChart->setLayoutPolicy( QwtPlotBarChart::AutoAdjustSamples );
    barChart->setLayoutHint( 4.0 ); // minimum width for a single bar
    barChart->setSpacing( 10 );     // spacing between bars
    barChart->setBaseline( 0 );
    auto* symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
    symbol->setLineWidth( 2 );
    symbol->setFrameStyle( QwtColumnSymbol::Raised );
    symbol->setPalette( QColor( 0, 50, 0 ) );
    barChart->setSymbol( symbol );

    // Create the line plots
    curvePlot[0] = new QwtPlotCurve( "" );
    curvePlot[1] = new QwtPlotCurve( "" );
    curvePlot[0]->setLegendIconSize( QSize( 0, 0 ) );
    curvePlot[1]->setLegendIconSize( QSize( 0, 0 ) );

    // Plot some initial data to initialize the plot
    plot( std::vector<float>( 10, 1.0 ) );
    plot( std::vector<float>( 1000, 1.0 ) );
}


LoadBalance::~LoadBalance()
{
    delete barChart;
    delete curvePlot[0];
    delete curvePlot[1];
}


void LoadBalance::plot( const std::vector<float>& time_ )
{
    PROFILE( "plot" );
    N_procs       = static_cast<int>( time_.size() );
    auto new_time = toQVector( time_ );
    if ( new_time == time ) {
        replot();
        return;
    }
    std::swap( time, new_time );
    if ( time.empty() ) {
        return;
    }

    // Copy the data and compute the mean
    rank.resize( N_procs );
    for ( int i = 0; i < N_procs; i++ )
        rank[i] = i;
    double mean = 0;
    for ( int i = 0; i < N_procs; i++ )
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
        barChart->setVisible( true );
        curvePlot[0]->attach( nullptr );
        curvePlot[1]->setPen( QColor( 204, 0, 0 ), 3, Qt::DotLine );
        range[0] = -0.5;
        range[1] = N_procs - 0.5;
    } else {
        barChart->attach( nullptr );
        curvePlot[0]->setSamples( rank, time );
        curvePlot[0]->attach( this );
        curvePlot[0]->setVisible( true );
        curvePlot[0]->setPen( QColor( 0, 50, 0 ), 3, Qt::SolidLine );
        curvePlot[1]->setPen( QColor( 204, 0, 0 ), 2, Qt::DotLine );
        range[0] = 0;
        range[1] = N_procs - 1;
    }
    QVector<double> rank2( 2 ), mean2( 2 );
    rank2[0] = range[0];
    rank2[1] = range[1];
    mean2[0] = mean;
    mean2[1] = mean;
    curvePlot[1]->setSamples( rank2, mean2 );
    curvePlot[1]->attach( this );
    curvePlot[1]->setVisible( true );
    insertLegend( new QwtLegend() );

    // Set the label
    int xaxis = QwtPlot::xBottom;
    int yaxis = QwtPlot::yLeft;
    setAxisTitle( xaxis, qwtText( "Rank", QFont( "Times", 10, QFont::Bold ) ) );
    setAxisTitle( yaxis, qwtText( "Time (s)", QFont( "Times", 10, QFont::Bold ) ) );

    // Set the axis range
    double max_time = 0;
    for ( double i : time )
        max_time = std::max( max_time, i );
    global_range = { { range[0], range[1], 0, 1.1 * max_time } };
    zoom( global_range );
}
void LoadBalance::zoom( const std::array<double, 4>& range )
{
    plot_range = range;
    // Create the rank ticks
    auto proc0 = static_cast<int>( ceil( range[0] ) );
    auto proc1 = static_cast<int>( floor( range[1] ) );
    QwtScaleDiv xtick( range[0], range[1] );
    if ( proc1 - proc0 < 16 ) {
        xtick = QwtScaleDiv( range[0], range[1], QList<double>(), QList<double>(), rank.toList() );
    } else {
        QList<double> ticks;
        for ( int i = proc0; i < proc1; i += ( proc1 - proc0 ) / 16 )
            ticks.push_back( i );
        ticks.push_back( proc1 );
        xtick = QwtScaleDiv( range[0], range[1], QList<double>(), QList<double>(), ticks );
    }
    // Set the axis
    int xaxis = QwtPlot::xBottom;
    int yaxis = QwtPlot::yLeft;
    setAxisScale( xaxis, range[0], range[1] );
    setAxisScaleDiv( xaxis, xtick );
    setAxisScale( yaxis, range[2], range[3] );
    setAxisScale( QwtPlot::xTop, 0, 0 );
    updateAxes();
    replot();
}


void LoadBalance::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "distrowatch.pdf" );
}


/***********************************************************************
 * Mouse events                                                         *
 ***********************************************************************/
void LoadBalance::mousePressEvent( QMouseEvent* event )
{
    if ( event->button() == Qt::LeftButton ) {
        active = true;
        start  = canvas->mapFromGlobal( event->globalPos() );
        last   = start;
        zoom_box->show();
        zoom_box->move( QRect( start.x(), start.y(), 0, 0 ) );
    } else if ( event->button() == Qt::RightButton ) {
    }
}
void LoadBalance::mouseMoveEvent( QMouseEvent* event )
{
    QPoint pos = canvas->mapFromGlobal( event->globalPos() );
    int dist   = std::max( abs( last.x() - pos.x() ), abs( last.y() - pos.y() ) );
    if ( !active || dist < 4 )
        return;
    last = pos;
    QRect box( std::min( start.x(), last.x() ), std::min( start.y(), last.y() ),
        abs( start.x() - last.x() ), abs( start.y() - last.y() ) );
    zoom_box->move( box );
}
void LoadBalance::mouseReleaseEvent( QMouseEvent* event )
{
    if ( event->button() == Qt::LeftButton ) {
        active = false;
        zoom_box->hide();
        QPoint pos = canvas->mapFromGlobal( event->globalPos() );
        if ( abs( start.x() - pos.x() ) < 10 || abs( start.y() - pos.y() ) < 10 )
            return;
        auto xmap = canvasMap( QwtPlot::xBottom );
        auto ymap = canvasMap( QwtPlot::yLeft );
        double x1 = xmap.invTransform( std::min( start.x(), pos.x() ) );
        double x2 = xmap.invTransform( std::max( start.x(), pos.x() ) );
        double y1 = ymap.invTransform( std::max( start.y(), pos.y() ) );
        double y2 = ymap.invTransform( std::min( start.y(), pos.y() ) );
        std::array<double, 4> range;
        range[0] = std::max( x1, global_range[0] );
        range[1] = std::min( x2, global_range[1] );
        range[2] = std::max( y1, global_range[2] );
        range[3] = std::min( y2, global_range[3] );
        zoom( range );
    }
}
