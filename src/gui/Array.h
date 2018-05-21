#ifndef Array_H
#define Array_H

#include <set>
#include <stdint.h>
#include <vector>


// Bool array
class BoolArray
{
public:
    BoolArray( int Nx = 0, int Ny = 1, int Nz = 1 ) : Ni( 0 ), data( nullptr )
    {
        resize( Nx, Ny, Nz );
    }
    BoolArray( const BoolArray& rhs ) : Ni( rhs.Ni ), data( nullptr )
    {
        resize( N[0], N[1], N[2] );
        memcpy( data, rhs.data, Ni * sizeof( uint64_t ) );
    }
    BoolArray& operator=( const BoolArray& rhs )
    {
        if ( this != &rhs ) {
            resize( N[0], N[1], N[2] );
            memcpy( data, rhs.data, Ni * sizeof( uint64_t ) );
        }
        return *this;
    }
    ~BoolArray() { delete[] data; }
    void resize( int Nx, int Ny = 1, int Nz = 1 )
    {
        delete[] data;
        data = nullptr;
        N[0] = Nx;
        N[1] = Ny;
        N[2] = Nz;
        Ni   = N[0] * N[1] * N[2] / 64;
        if ( N[0] * N[1] * N[2] - 64 * Ni > 0 ) {
            Ni++;
        }
        if ( Ni > 0 ) {
            data = new uint64_t[Ni];
            memset( data, 0, Ni * sizeof( uint64_t ) );
        }
    }
    bool operator()( int i = 0, int j = 0, int k = 0 ) const
    {
        int ijk = i + j * N[0] + k * N[0] * N[1];
        return ( data[ijk >> 6] >> ( ijk & 0x3F ) ) & 0x1;
    }
    void set( int i, int j = 0, int k = 0 )
    {
        int ijk       = i + j * N[0] + k * N[0] * N[1];
        uint64_t mask = ( (uint64_t) 0x1 ) << ( ijk & 0x3F );
        data[ijk >> 6] |= mask;
    }
    void clear( int i, int j = 0, int k = 0 )
    {
        int ijk       = i + j * N[0] + k * N[0] * N[1];
        uint64_t mask = ( (uint64_t) 0x1 ) << ( ijk & 0x3F );
        data[ijk >> 6] &= mask;
    }
    int size( int d ) { return N[d]; }

protected:
    int N[3];
    size_t Ni;
    uint64_t* data;
};


// Simple matrix class
template<class TYPE>
class Matrix
{
public:
    typedef std::shared_ptr<Matrix<TYPE>> shared_ptr;
    int N;
    int M;
    TYPE* data;
    Matrix() : N( 0 ), M( 0 ), data( nullptr ) {}
    Matrix( int N_, int M_ ) : N( N_ ), M( M_ ), data( new TYPE[N_ * M_] ) { fill( 0 ); }
    ~Matrix() { delete[] data; }
    TYPE& operator()( int i, int j ) { return data[i + j * N]; }
    void fill( TYPE x )
    {
        for ( int i = 0; i < N * M; i++ )
            data[i] = x;
    }
    TYPE min() const
    {
        TYPE x = data[0];
        for ( int i = 0; i < N * M; i++ )
            x = std::min( x, data[i] );
        return x;
    }
    TYPE max() const
    {
        TYPE x = data[0];
        for ( int i = 0; i < N * M; i++ )
            x = std::max( x, data[i] );
        return x;
    }
    TYPE sum() const
    {
        TYPE x = 0;
        for ( int i = 0; i < N * M; i++ )
            x += data[i];
        return x;
    }
    double mean() const
    {
        TYPE x = sum();
        return static_cast<double>( x ) / ( N * M );
    }

protected:
    Matrix( const Matrix& );            // Private copy constructor
    Matrix& operator=( const Matrix& ); // Private assignment operator
};

#endif
