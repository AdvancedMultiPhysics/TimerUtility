#include "QSplitterGrid.h"

#include <QSignalMapper>
#include <QSplitter>
#include <QTextEdit>

#include <iostream>

// Define NULL_USE
#define NULL_USE( variable )                \
    do {                                    \
        if ( 0 ) {                          \
            char* temp = (char*) &variable; \
            temp++;                         \
        }                                   \
    } while ( 0 )


template<class T>
static inline int sum( const std::vector<T>& x )
{
    T y = 0;
    for ( size_t i = 0; i < x.size(); i++ )
        y += x[i];
    return y;
}


/***********************************************************************
 * Constructor/destructor                                               *
 ***********************************************************************/
QSplitterGrid::QSplitterGrid( QWidget* parent )
    : QScrollArea( parent ),
      grid( NULL ),
      hspacing( 1 ),
      vspacing( 1 ),
      row_uniform( false ),
      col_uniform( false ),
      frame_widget( NULL )
{
    reset();
}
QSplitterGrid::~QSplitterGrid()
{
    takeWidget();
    delete frame_widget;
}
void QSplitterGrid::reset()
{
    for ( auto ptr : row_boundaries ) {
        delete ptr;
    }
    for ( auto ptr : col_boundaries ) {
        delete ptr;
    }
    row_boundaries.clear();
    col_boundaries.clear();
    pos_map.clear();
    widget_map.clear();
    takeWidget();
    delete frame_widget;
    grid         = new QGridLayout();
    frame_widget = new QWidget;
    frame_widget->setLayout( grid );
    this->setWidget( frame_widget );
    this->setWidgetResizable( true );
    setHorizontalSpacing( hspacing );
    setVerticalSpacing( vspacing );
}


/***********************************************************************
 * Set spacing                                                          *
 ***********************************************************************/
void QSplitterGrid::setSpacing( int spacing )
{
    setVerticalSpacing( spacing );
    setHorizontalSpacing( spacing );
}
void QSplitterGrid::setVerticalSpacing( int spacing )
{
    vspacing = spacing;
    grid->setVerticalSpacing( vspacing );
}
void QSplitterGrid::setHorizontalSpacing( int spacing )
{
    hspacing = spacing;
    grid->setHorizontalSpacing( hspacing );
}


/***********************************************************************
 * Add a widget                                                         *
 ***********************************************************************/
void QSplitterGrid::tableSize( int N_rows, int N_columns )
{
    reset();
    QSize box = frame_widget->size();
    row_size.resize( N_rows );
    for ( int i = 0; i < N_rows; i++ )
        row_size[i] = box.height() / N_rows;
    col_size.resize( N_columns );
    for ( int i = 0; i < N_columns; i++ )
        col_size[i] = box.width() / N_columns;
    min_row_size = std::vector<int>( N_rows, 0 );
    min_col_size = std::vector<int>( N_columns, 0 );
    row_boundaries.resize( N_rows );
    for ( size_t i = 0; i < row_boundaries.size(); i++ )
        row_boundaries[i] =
            new QSplitterGridLineClass( Qt::SizeVerCursor, i + 1, frame_widget, this );
    col_boundaries.resize( N_columns );
    for ( size_t i = 0; i < col_boundaries.size(); i++ )
        col_boundaries[i] =
            new QSplitterGridLineClass( Qt::SizeHorCursor, -( i + 1 ), frame_widget, this );
}
void QSplitterGrid::addWidget( QWidget* widget, int row, int col )
{
    grid->addWidget( widget, row, col );
    pos_map[widget] = std::pair<int, int>( row, col );
    // widget->setMinimumSize(0,0);
    min_row_size[row] = std::max<int>( min_row_size[row], widget->minimumSize().height() );
    min_col_size[col] = std::max<int>( min_col_size[col], widget->minimumSize().width() );
    widget_map[std::pair<int, int>( row, col )] = widget;
}


/***********************************************************************
 * Move boundaries / resize widget                                      *
 ***********************************************************************/
static inline void calcMoveBoundaryShift( int index, int pos, int space, bool uniform,
    const std::vector<int>& size, const std::vector<int>& min_size, int& p0, int& shift )
{
    p0 = 0;
    for ( int i = 0; i <= index; i++ )
        p0 += size[i];
    p0 += index * space;
    shift = pos - p0;
    shift = std::max( shift, -( size[index] - min_size[index] ) );
    if ( !uniform ) {
        if ( index < (int) size.size() - 1 )
            shift = std::min( shift, size[index + 1] - min_size[index + 1] );
        else
            shift = std::min( shift, 100 );
    }
}
void QSplitterGrid::moveBoundary( int index0, int pos, bool final )
{
    if ( index0 < 0 ) {
        // We are moving a column
        int index = -index0 - 1;
        int p0, shift;
        calcMoveBoundaryShift(
            index, pos, vspacing, col_uniform, col_size, min_col_size, p0, shift );
        col_boundaries[index]->move( 0, p0 + shift );
        grid->setColumnMinimumWidth( index, col_size[index] + shift );
        if ( index < (int) col_size.size() - 1 )
            grid->setColumnMinimumWidth( index + 1, col_size[index + 1] - shift );
        if ( final ) {
            col_size[index] += shift;
            if ( col_uniform ) {
                col_size = std::vector<int>( col_size.size(), std::max( col_size[index], 5 ) );
            } else {
                if ( index < (int) col_size.size() - 1 )
                    col_size[index + 1] -= shift;
            }
        }
    } else {
        // We are moving a row
        int index = index0 - 1;
        int p0, shift;
        calcMoveBoundaryShift(
            index, pos, hspacing, row_uniform, row_size, min_row_size, p0, shift );
        row_boundaries[index]->move( p0 + shift, 0 );
        grid->setRowMinimumHeight( index, row_size[index] + shift );
        if ( index < (int) row_size.size() - 1 )
            grid->setRowMinimumHeight( index + 1, row_size[index + 1] - shift );
        if ( final ) {
            row_size[index] += shift;
            if ( row_uniform ) {
                row_size = std::vector<int>( row_size.size(), std::max( row_size[index], 5 ) );
            } else {
                if ( index < (int) row_size.size() - 1 )
                    row_size[index + 1] -= shift;
            }
        }
    }
    if ( final )
        resize2();
}
void QSplitterGrid::resize( int w, int h )
{
    if ( w > 0 ) {
        int s = sum( row_size );
        for ( size_t i = 0; i < row_size.size(); i++ )
            row_size[i] = std::max( ( h * row_size[i] - vspacing ) / s, 2 );
    }
    if ( h > 0 ) {
        int s = sum( col_size );
        for ( size_t i = 0; i < col_size.size(); i++ )
            col_size[i] = std::max( ( w * col_size[i] - hspacing ) / s, 2 );
    }
    resize2();
}
void QSplitterGrid::resize2()
{
    for ( size_t i = 0; i < row_size.size(); i++ )
        grid->setRowMinimumHeight( i, row_size[i] );
    for ( size_t i = 0; i < col_size.size(); i++ )
        grid->setColumnMinimumWidth( i, col_size[i] );
    int w2 = sum( col_size ) + hspacing * ( col_size.size() - 1 );
    int h2 = sum( row_size ) + vspacing * ( row_size.size() - 1 );
    w2     = std::max( w2, 0 );
    h2     = std::max( h2, 0 );
    frame_widget->setMinimumSize( w2, h2 );
    frame_widget->resize( w2, h2 );
    int pos = 0;
    for ( size_t i = 0; i < row_boundaries.size(); i++ ) {
        pos += row_size[i];
        row_boundaries[i]->setGeometry( 0, pos, w2, 20 );
    }
    pos = 0;
    for ( size_t i = 0; i < col_boundaries.size(); i++ ) {
        pos += col_size[i];
        col_boundaries[i]->setGeometry( pos, 0, 20, h2 );
    }
    for ( auto fun : resizeCallBack )
        fun();
}
QRect QSplitterGrid::getPosition( int row, int col ) const
{
    QRect pos( 0, 0, 0, 0 );
    if ( row < (int) row_size.size() && col < (int) col_size.size() )
        pos = grid->itemAtPosition( row, col )->geometry();
    return pos;
}
const QWidget* QSplitterGrid::getWidget( int row, int column ) const
{
    auto it = widget_map.find( std::pair<int, int>( row, column ) );
    if ( it == widget_map.end() )
        return NULL;
    return it->second;
}
void QSplitterGrid::setRowHeight( int size )
{
    for ( size_t i = 0; i < row_size.size(); i++ )
        row_size[i] = size;
    resize2();
}
void QSplitterGrid::setRowHeight( const std::vector<int>& size )
{
    for ( size_t i = 0; i < std::min( row_size.size(), size.size() ); i++ )
        row_size[i] = size[i];
    resize2();
}
void QSplitterGrid::setColumnWidth( int size )
{
    for ( size_t i = 0; i < col_size.size(); i++ )
        col_size[i] = size;
    resize2();
}
void QSplitterGrid::setColumnWidth( const std::vector<int>& size )
{
    for ( size_t i = 0; i < std::min( col_size.size(), size.size() ); i++ )
        col_size[i] = size[i];
    resize2();
}
