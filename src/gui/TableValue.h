#ifndef TableValue_H
#define TableValue_H

#include <QTableWidget>


// stringf
inline std::string stringf( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    char tmp[1024];
    vsprintf( tmp, format, ap );
    va_end( ap );
    return std::string( tmp );
}


// Class to hold a table value that will be compared by magnitude
class TableValue : public QTableWidgetItem
{
public:
    template<class TYPE>
    TableValue( TYPE value, const char* format )
        : QTableWidgetItem( stringf( format, value ).c_str() ), x( static_cast<double>( value ) )
    {
    }
    TableValue()                    = delete;
    TableValue( const TableValue& ) = delete;
    TableValue& operator=( const TableValue& ) = delete;
    virtual ~TableValue() {}
    virtual bool operator<( const QTableWidgetItem& other ) const
    {
        const TableValue* rhs = dynamic_cast<const TableValue*>( &other );
        if ( rhs )
            return x < rhs->x;
        return QTableWidgetItem::operator<( other );
    }

private:
    double x;
};


#endif
