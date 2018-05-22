#include "MemoryPlot.h"
#include "ProfilerApp.h"
#include "colormap.h"
#include <algorithm>
#include <qwt_column_symbol.h>
#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_barchart.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_renderer.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_widget.h>


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

// Subroutine to find the first element in X which is greater than Y using a simple hashing
// technique
inline size_t findfirst( const std::vector<double>& X, double Y )
{
    if ( X.empty() )
        return 0;
    if ( X[0] >= Y )
        return 0;
    if ( X[X.size() - 1] < Y )
        return X.size();
    size_t lower = 0;
    size_t upper = X.size() - 1;
    while ( ( upper - lower ) != 1 ) {
        size_t value = ( upper + lower ) / 2;
        if ( X[value] >= Y )
            upper = value;
        else
            lower = value;
    }
    return upper;
}


MemoryPlot::MemoryPlot( QWidget* parent, const std::vector<MemoryResults>& memory_ )
    : QwtPlot( parent ),
      d_t( std::array<double, 2>{ { 0, 0 } } ),
      d_offset( 0 ),
      d_scale( 0 ),
      d_last_rank( -10 ),
      d_memory( &memory_ ),
      d_N_procs( memory_.size() )
{
    setTitle( qwtText( "", QFont( "Times", 12, QFont::Bold ) ) );
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

    auto* canvas = new QwtPlotCanvas();
    canvas->setLineWidth( 0 );
    canvas->setFrameStyle( QFrame::Box | QFrame::Sunken );
    canvas->setBorderRadius( 0 );
    QPalette canvasPalette( QColor( 230, 255, 255 ) );
    canvasPalette.setColor( QPalette::Foreground, QColor( 230, 255, 255 ) );
    canvas->setPalette( canvasPalette );
    canvas->setContentsMargins( 0, 0, 0, 0 );
    setCanvas( canvas );

    // Create the line plots
    d_curvePlot.resize( d_N_procs, nullptr );
    for ( int i = 0; i < d_N_procs; i++ ) {
        d_curvePlot[i] = new QwtPlotCurve( "" );
        d_curvePlot[i]->setLegendIconSize( QSize( 0, 0 ) );
    }
    d_time.resize( d_N_procs );
    d_size.resize( d_N_procs );
}


MemoryPlot::~MemoryPlot()
{
    for ( auto& i : d_curvePlot ) {
        i->attach( nullptr );
        delete i;
    }
}


void MemoryPlot::plot( std::array<double, 2> t_in, int rank )
{
    if ( t_in == d_t && rank == d_last_rank )
        return;
    PROFILE_START( "plot" );
    d_t         = t_in;
    d_last_rank = rank;

    // Get the offset
    d_offset = 0.0;
    if ( fabs( d_t[1] - d_t[0] ) < 10e-3 ) {
        d_offset = 1e-3 * floor( d_t[0] * 1e3 );
        d_scale  = 1e3;
    }

    // Get the memory usage for each rank
    for ( int i = 0; i < d_N_procs; i++ ) {
        d_curvePlot[i]->attach( nullptr );
        d_curvePlot[i]->setVisible( false );
    }
    std::array<double, 2> range = { { 1e100, 0 } };
    if ( d_N_procs == 0 ) {
        // Dummy plot
    } else if ( rank == -1 ) {
        for ( int i = 0; i < d_N_procs; i++ ) {
            auto tmp = updateRankData( i );
            range[0] = std::min<double>( range[0], tmp[0] );
            range[1] = std::max<double>( range[1], tmp[1] );
        }
    } else {
        auto tmp = updateRankData( rank );
        range[0] = std::min<double>( range[0], tmp[0] );
        range[1] = std::max<double>( range[1], tmp[1] );
    }

    // Scale and plot the data for each rank
    double xscale = 1.0, yscale = 1.0;
    std::string xlabel, ylabel;
    if ( range[1] < 1e6 ) {
        yscale = 1.0 / 1024.0;
        ylabel = "Memory (kB)";
    } else if ( range[1] < 1e9 ) {
        yscale = 1.0 / ( 1024.0 * 1024.0 );
        ylabel = "Memory (MB)";
    } else {
        yscale = 1.0 / ( 1024.0 * 1024.0 * 1024.0 );
        ylabel = "Memory (GB)";
    }
    if ( d_offset != 0.0 ) {
        std::string offset_label =
            " (offset = " + std::to_string( static_cast<uint64_t>( 1e3 * d_offset ) ) + " ms)";
        if ( fabs( d_t[1] - d_t[0] ) < 50e-6 ) {
            xscale = 1e6;
            xlabel = "Time (us)" + offset_label;
        } else {
            xscale = 1e3;
            xlabel = "Time (ms)" + offset_label;
        }
    } else {
        xscale = 1.0;
        xlabel = "Time (s)";
    }
    range[0] *= yscale;
    range[1] *= yscale;
    if ( d_N_procs == 0 ) {
        // Dummy plot
    } else if ( rank == -1 ) {
        for ( int i = 0; i < d_N_procs; i++ )
            plotRank( i, yscale );
    } else {
        plotRank( rank, yscale );
    }
    insertLegend( new QwtLegend() );

    // Set the axis
    int xaxis = QwtPlot::xBottom;
    int yaxis = QwtPlot::yLeft;
    setAxisScale( xaxis, xscale * ( d_t[0] - d_offset ), xscale * ( d_t[1] - d_offset ) );
    setAxisTitle( xaxis, qwtText( xlabel.c_str(), QFont( "Times", 10, QFont::Bold ) ) );
    if ( range[1] > 0 ) {
        setAxisScale( yaxis, 0.95 * range[0], 1.05 * range[1] );
        setAxisTitle( yaxis, qwtText( ylabel.c_str(), QFont( "Times", 10, QFont::Bold ) ) );
    } else {
        setAxisScale( yaxis, 0, 0 );
    }
    setAxisScale( QwtPlot::xTop, 0, 0 );
    replot();
    PROFILE_STOP( "plot" );
}


std::array<size_t, 2> MemoryPlot::updateRankData( int rank )
{
    auto& time = d_memory->operator[]( rank ).time;
    auto& size = d_memory->operator[]( rank ).bytes;
    size_t i1  = findfirst( time, d_t[0] );
    size_t i2  = findfirst( time, d_t[1] );
    i2         = std::min( i2, time.size() - 1 );
    size_t N2  = ( ( i2 - i1 + 1 ) / 5000 ) + 1; // Limit plot to ~5000 points
    size_t N   = ( i2 - i1 + 1 ) / N2;
    size_t i0  = std::max<uint64_t>( i1, 1 ) - 1;
    d_time[rank].clear();
    d_size[rank].clear();
    d_time[rank].reserve( 2 * N + 2 );
    d_size[rank].reserve( 2 * N + 2 );
    d_time[rank].push_back( d_t[0] );
    d_size[rank].push_back( size[i0] );
    std::array<size_t, 2> range = { { size[i0], size[i0] } };
    for ( size_t i = i1; i < i2; i += N2 ) {
        d_time[rank].push_back( time[i] );
        d_size[rank].push_back( size[i] );
        range[0] = std::min<uint64_t>( range[0], size[i] );
        range[1] = std::max<uint64_t>( range[1], size[i] );
    }
    d_time[rank].push_back( d_t[1] );
    d_size[rank].push_back( d_size[rank].back() );
    if ( d_offset != 0.0 ) {
        for ( auto& t : d_time[rank] )
            t = d_scale * ( t - d_offset );
    }
    return range;
}
void MemoryPlot::plotRank( int rank, double scale )
{
    for ( double& i : d_size[rank] )
        i *= scale;
    d_curvePlot[rank]->setSamples( d_time[rank].data(), d_size[rank].data(), d_time[rank].size() );
    d_curvePlot[rank]->attach( this );
    d_curvePlot[rank]->setVisible( true );
    uint32_t color = jet( 0 );
    if ( d_N_procs > 1 )
        color = jet( static_cast<double>( rank ) / static_cast<double>( d_N_procs - 1 ) );
    d_curvePlot[rank]->setPen( QColor( color ), 2, Qt::SolidLine );
}


void MemoryPlot::align( int left, int right )
{
    QRectF xleft  = plotLayout()->scaleRect( QwtPlot::yLeft );
    QRectF xright = plotLayout()->scaleRect( QwtPlot::yRight );
    QRect geom    = this->geometry();
    int left2     = left - ( geom.left() + xleft.width() );
    int right2    = ( geom.left() + geom.width() - xright.width() ) - right;
    right2 /= 2;
    if ( left == -1 ) {
        left2 = 0;
    }
    if ( right == -1 ) {
        right2 = 0;
    }
    setContentsMargins( left2, 0, right2, 0 );
    // replot();
}


void MemoryPlot::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "distrowatch.pdf" );
}
