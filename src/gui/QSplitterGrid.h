#ifndef QSplitterGrid_H
#define QSplitterGrid_H

#include <QCoreApplication>
#include <QGridLayout>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSignalMapper>
#include <QSplitter>

#include <functional>
#include <map>


class QSplitterGridLineClass;


//! This is a class to get a grid-based splitter
class QSplitterGrid : public QScrollArea
{
    Q_OBJECT

public:
    //! Default constructor
    explicit QSplitterGrid( QWidget* parent = nullptr );

    //! Destructor
    virtual ~QSplitterGrid();

    //! Reset all internal data
    void reset();

    //! Set the table size
    void tableSize( int N_rows, int N_columns );

    //! Get the number of rows
    inline int numberRows() const { return row_size.size(); }

    //! Get the number of columns
    inline int numberColumns() const { return col_size.size(); }

    //! Add a widget at the given row, column (note: takes ownership)
    void addWidget( QWidget* widget, int row, int column );

    //! Set the default spacing
    void setSpacing( int spacing );

    //! Set the vertical spacing
    void setVerticalSpacing( int spacing );

    //! Set the horizontal spacing
    void setHorizontalSpacing( int spacing );

    //! Set the vertical spacing
    inline int getVerticalSpacing() { return vspacing; }

    //! Set the horizontal spacing
    inline int getHorizontalSpacing() { return hspacing; }

    //! Set the row height for all rows
    void setRowHeight( int size );

    //! Set the row height for all rows
    void setRowHeight( const std::vector<int>& size );

    //! Set the row visibility
    void setRowVisible( const std::vector<bool>& visible );

    //! Get the row height for all rows
    const std::vector<int>& getRowHeight() const { return row_size; }

    //! Set the rows to have a uniform height?
    inline void setUniformRowHeight( bool uniform ) { row_uniform = uniform; }

    //! Set the column width for all columns
    void setColumnWidth( int size );

    //! Set the column width for all columns
    void setColumnWidth( const std::vector<int>& size );

    //! Set the column visibility
    void setColumnVisible( const std::vector<bool>& visible );

    //! Get the column width for all columns
    inline const std::vector<int>& getColumnWidth() const { return col_size; }

    //! Set the columns to have a uniform width?
    inline void setUniformColumnWidth( bool uniform ) { col_uniform = uniform; }

    //! Get the position of the cell within the frame
    QRect getPosition( int row, int column ) const;

    //! Get the position of the cell within the frame
    const QWidget* getWidget( int row, int column ) const;

    //! Resize the widget
    virtual void resize( const QSize& size ) { resize( size.width(), size.height() ); }

    //! Resize the widget
    virtual void resize( int w, int h );

    //! Register callback when table is resized
    inline void registerResizeCallback( std::function<void()> fun )
    {
        resizeCallBack.push_back( fun );
    }

private:
    QGridLayout* grid;
    int hspacing, vspacing;
    bool row_uniform, col_uniform;
    QSignalMapper* signalMapper;
    QWidget* frame_widget;
    std::map<QWidget*, std::pair<int, int>> pos_map;
    std::map<std::pair<int, int>, QWidget*> widget_map;
    std::vector<QSplitterGridLineClass*> row_boundaries;
    std::vector<QSplitterGridLineClass*> col_boundaries;
    std::vector<bool> row_visible;
    std::vector<bool> col_visible;
    std::vector<int> row_size;
    std::vector<int> col_size;
    std::vector<int> min_row_size;
    std::vector<int> min_col_size;
    std::vector<std::function<void()>> resizeCallBack;
    void resize2();

protected:
    void moveBoundary( int index, int pos, bool final );

    friend class QSplitter2;
    friend class QSplitterGridLineClass;
};


// Class to create a movable (transparent) line
class QSplitterGridLineClass : public QWidget
{
    Q_OBJECT
public:
    QSplitterGridLineClass( Qt::CursorShape cur, int index_, QWidget* parent, QSplitterGrid* grid_ )
        : QWidget( parent ),
          grid( grid_ ),
          type( cur ),
          index( index_ ),
          active( false ),
          start( 0 ),
          last( 0 ),
          w( 0 ),
          h( 0 )
    {
        show();
    }
    virtual ~QSplitterGridLineClass() {}

protected:
    inline int getPos( QMouseEvent* event ) const
    {
        int pos = 0;
        if ( type == Qt::SizeVerCursor ) {
            pos = parentWidget()->mapFromGlobal( event->globalPos() ).y();
            pos = std::max( std::min( pos, h + 200 ), 0 );
        } else if ( type == Qt::SizeHorCursor ) {
            pos = parentWidget()->mapFromGlobal( event->globalPos() ).x();
            pos = std::max( std::min( pos, w + 200 ), 0 );
        }
        return pos;
    }
    virtual void enterEvent( QEvent* ) { setCursor( type ); }
    virtual void leaveEvent( QEvent* ) { setCursor( Qt::ArrowCursor ); }
    virtual void mousePressEvent( QMouseEvent* event )
    {
        if ( event->button() == Qt::LeftButton ) {
            start  = getPos( event );
            last   = start;
            w      = parentWidget()->geometry().width();
            h      = parentWidget()->geometry().height();
            active = true;
        }
    }
    virtual void mouseMoveEvent( QMouseEvent* event )
    {
        int pos = getPos( event );
        if ( !active || abs( last - pos ) < 4 )
            return;
        last = pos;
        grid->moveBoundary( index, pos, false );
        QCoreApplication::processEvents();
    }
    virtual void mouseReleaseEvent( QMouseEvent* event )
    {
        if ( event->button() == Qt::LeftButton ) {
            int pos = getPos( event );
            grid->moveBoundary( index, pos, true );
            QCoreApplication::processEvents();
        }
    }

protected:
    QSplitterGrid* grid;
    Qt::CursorShape type;
    int index;
    bool active;
    int start, last, w, h;
};


#endif
