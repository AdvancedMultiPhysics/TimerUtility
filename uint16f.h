#ifndef included_uint16f
#define included_uint16f


/** \class uint16f
 *
 * Class to store an unsigned integer as a half precision floating type.
 */
class uint16f
{
public:
    // Constructors
    constexpr uint16f() : data( 0 ) {}
    explicit inline constexpr uint16f( uint64_t x ) : data( getData( x ) ) {}
    // Comparison operators
    inline constexpr bool operator==( const uint16f& rhs ) const { return data == rhs.data; }
    inline constexpr bool operator!=( const uint16f& rhs ) const { return data != rhs.data; }
    inline constexpr bool operator>=( const uint16f& rhs ) const { return data >= rhs.data; }
    inline constexpr bool operator<=( const uint16f& rhs ) const { return data <= rhs.data; }
    inline constexpr bool operator>( const uint16f& rhs ) const { return data > rhs.data; }
    inline constexpr bool operator<( const uint16f& rhs ) const { return data < rhs.data; }
    // Overload typecast
    inline constexpr operator uint64_t() const;

private:
    uint16_t data;
    static inline constexpr uint16_t getData( uint64_t x );
};


/***********************************************************************
 * uint16f                                                              *
 ***********************************************************************/
constexpr uint16_t uint16f::getData( uint64_t x )
{
    uint16_t y = static_cast<uint16_t>( x );
    if ( x > 2048 ) {
        uint64_t tmp = x >> 11;
        uint16_t e   = 0;
        while ( tmp != 0 ) {
            e++;
            tmp >>= 1;
        }
        uint16_t b = x >> ( e - 1 ) & 0x7FF;
        y          = ( e << 11 ) | b;
        if ( e >= 32 )
            y = 0xFFFF;
    }
    return y;
}
constexpr uint16f::operator uint64_t() const
{
    uint16_t e = data >> 11;
    uint16_t b = data & 0x7FF;
    uint64_t x = b;
    if ( e > 0 ) {
        x = x | 0x800;
        x <<= ( e - 1 );
    }
    return x;
}


/***********************************************************************
 * numeric_limits<uint16f>                                              *
 ***********************************************************************/
namespace std {
template<>
class numeric_limits<uint16f>
{
public:
    static constexpr bool is_specialized() { return true; }
    static constexpr bool is_signed() { return false; }
    static constexpr bool is_integer() { return true; }
    static constexpr bool is_exact() { return false; }
    static constexpr bool has_infinity() { return false; }
    static constexpr bool has_quiet_NaN() { return false; }
    static constexpr bool has_signaling_NaN() { return false; }
    static constexpr bool has_denorm() { return false; }
    static constexpr bool has_denorm_loss() { return false; }
    static constexpr std::float_round_style round_style() { return round_toward_zero; }
    static constexpr bool is_iec559() { return false; }
    static constexpr bool is_bounded() { return true; }
    static constexpr bool is_modulo() { return false; }
    static constexpr int digits() { return 11; }
    static constexpr int digits10() { return 3; }
    static constexpr int max_digits10() { return 4; }
    static constexpr int radix() { return 2; }
    static constexpr int min_exponent() { return 0; }
    static constexpr int min_exponent10() { return 0; }
    static constexpr int max_exponent() { return 42; }
    static constexpr int max_exponent10() { return 13; }
    static constexpr bool traps() { return false; }
    static constexpr bool tinyness_before() { return false; }
    static constexpr uint16f min() { return uint16f( 0 ); }
    static constexpr uint16f lowest() { return uint16f( 0 ); }
    static constexpr uint16f max() { return uint16f( ~( (uint64_t) 0 ) ); }
    static constexpr uint16f epsilon() { return uint16f( 1 ); }
    static constexpr uint16f round_error() { return uint16f( 0 ); }
    static constexpr uint16f infinity() { return uint16f( 0 ); }
    static constexpr uint16f quiet_NaN() { return uint16f( 0 ); }
    static constexpr uint16f signaling_NaN() { return uint16f( 0 ); }
    static constexpr uint16f denorm_min() { return uint16f( 0 ); }
};
} // namespace std


#endif
