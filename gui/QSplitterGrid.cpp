#include "QSplitterGrid.h"

#include <QSignalMapper>
#include <QSplitter>
#include <QTextEdit>

#include <iostream>

#include "ProfilerApp.h"


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
      grid( nullptr ),
      hspacing( 1 ),
      vspacing( 1 ),
      row_uniform( false ),
      col_uniform( false ),
      frame_widget( nullptr )
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
    old_size = { 0, 0 };
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
    PROFILE_START( "tableSize" );
    reset();
    QSize box   = frame_widget->size();
    col_visible = std::vector<bool>( N_columns, true );
    row_visible = std::vector<bool>( N_rows, true );
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
    PROFILE_STOP( "tableSize" );
}
void QSplitterGrid::addWidget( QWidget* widget, int row, int col )
{
    grid->addWidget( widget, row, col );
    pos_map[widget]   = std::pair<int, int>( row, col );
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
        int s = std::max( sum( row_size ), 1 );
        for ( int& i : row_size )
            i = std::max( ( h * i - vspacing ) / s, 2 );
    }
    if ( h > 0 ) {
        int s = std::max( sum( col_size ), 1 );
        for ( int& i : col_size )
            i = std::max( ( w * i - hspacing ) / s, 2 );
    }
    resize2();
}
void QSplitterGrid::resize2()
{
    PROFILE_START( "resize" );
    // Get the row and column heights
    PROFILE_START( "resize-row_col" );
    std::array<int, 2> size = { 0, 0 };
    for ( size_t i = 0; i < col_size.size(); i++ ) {
        if ( row_visible[i] ) {
            grid->setColumnMinimumWidth( i, col_size[i] );
            size[0] += col_size[i] + hspacing;
        } else {
            grid->setColumnMinimumWidth( i, 0 );
        }
    }
    for ( size_t i = 0; i < row_size.size(); i++ ) {
        if ( row_visible[i] ) {
            grid->setRowMinimumHeight( i, row_size[i] );
            size[1] += row_size[i] + vspacing;
        } else {
            grid->setRowMinimumHeight( i, 0 );
        }
    }
    PROFILE_STOP( "resize-row_col" );
    // Set visibility of objects
    PROFILE_START( "resize-visible" );
    for ( size_t i = 0; i < row_size.size(); i++ ) {
        for ( size_t j = 0; j < col_size.size(); j++ ) {
            auto ptr = widget_map[std::pair<int, int>( i, j )];
            if ( ptr )
                ptr->setVisible( row_visible[i] && col_visible[j] );
        }
    }
    PROFILE_STOP( "resize-visible" );
    // Set position of boundaries
    PROFILE_START( "resize-boundaries" );
    for ( int i = 0, pos = 0; i < (int) row_boundaries.size(); i++ ) {
        if ( row_visible[i] ) {
            pos += row_size[i];
            row_boundaries[i]->setGeometry( 0, pos, size[0], 20 );
            row_boundaries[i]->setVisible( true );
        } else {
            row_boundaries[i]->setVisible( false );
        }
    }
    for ( int i = 0, pos = 0; i < (int) col_boundaries.size(); i++ ) {
        if ( col_visible[i] ) {
            pos += col_size[i];
            col_boundaries[i]->setGeometry( pos, 0, 20, size[1] );
            col_boundaries[i]->setVisible( true );
        } else {
            col_boundaries[i]->setVisible( false );
        }
    }
    PROFILE_STOP( "resize-boundaries" );
    // Resize frame
    PROFILE_START( "resize-frame" );
    if ( old_size != size )
        frame_widget->resize( size[0], size[1] );
    old_size = size;
    PROFILE_STOP( "resize-frame" );
    // Execute callbacks
    for ( auto fun : resizeCallBack )
        fun();
    PROFILE_STOP( "resize" );
}
QRect QSplitterGrid::getPosition( int row, int col ) const
{
    QRect pos( 0, 0, 0, 0 );
    if ( row < (int) row_size.size() && col < (int) col_size.size() ) {
        auto ptr = grid->itemAtPosition( row, col );
        if ( ptr && row_visible[row] && col_visible[col] )
            pos = ptr->geometry();
    }
    return pos;
}
const QWidget* QSplitterGrid::getWidget( int row, int column ) const
{
    auto it = widget_map.find( std::pair<int, int>( row, column ) );
    if ( it == widget_map.end() )
        return nullptr;
    return it->second;
}
void QSplitterGrid::setRowHeight( int size )
{
    for ( int& i : row_size )
        i = size;
    resize2();
}
void QSplitterGrid::setRowHeight( const std::vector<int>& size )
{
    for ( size_t i = 0; i < std::min( row_size.size(), size.size() ); i++ )
        row_size[i] = size[i];
    resize2();
}
void QSplitterGrid::setRowVisible( const std::vector<bool>& visible )
{
    for ( size_t i = 0; i < std::min( row_visible.size(), visible.size() ); i++ )
        row_visible[i] = visible[i];
    resize2();
}
void QSplitterGrid::setColumnWidth( int size )
{
    for ( int& i : col_size )
        i = size;
    resize2();
}
void QSplitterGrid::setColumnWidth( const std::vector<int>& size )
{
    for ( size_t i = 0; i < std::min( col_size.size(), size.size() ); i++ )
        col_size[i] = size[i];
    resize2();
}
void QSplitterGrid::setColumnVisible( const std::vector<bool>& visible )
{
    for ( size_t i = 0; i < std::min( col_visible.size(), visible.size() ); i++ )
        col_visible[i] = visible[i];
    resize2();
}
