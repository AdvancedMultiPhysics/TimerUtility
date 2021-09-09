// clang-format off

/*! \def DISABLE_WARNINGS
 *  \brief Reenable warnings
 *  \details This will re-enable warnings after a call to DIASABLE_WARNINGS
 */
/*! \def ENABLE_WARNINGS
 *  \brief Supress all warnings
 *  \details This will start to supress all compile warnings.
 *      Be sure to follow with ENABLE_WARNINGS
 */
#ifndef DISABLE_WARNINGS
    #if defined( __clang__ )
        #define DISABLE_WARNINGS                                                      \
            _Pragma( "clang diagnostic push" )                                        \
            _Pragma( "clang diagnostic ignored \"-Wall\"" )                           \
            _Pragma( "clang diagnostic ignored \"-Wextra\"" )                         \
            _Pragma( "clang diagnostic ignored \"-Wunused-private-field\"" )          \
            _Pragma( "clang diagnostic ignored \"-Wdeprecated-declarations\"" )       \
            _Pragma( "clang diagnostic ignored \"-Winteger-overflow\"" )              \
            _Pragma( "clang diagnostic ignored \"-Winconsistent-missing-override\"" ) \
            _Pragma( "clang diagnostic ignored \"-Wimplicit-int-float-conversion\"" )
        #define ENABLE_WARNINGS _Pragma( "clang diagnostic pop" )
    #elif defined( __GNUC__ )
        #define DISABLE_WARNINGS                                                \
            _Pragma( "GCC diagnostic push" )                                    \
            _Pragma( "GCC diagnostic ignored \"-Wpragmas\"" )                   \
            _Pragma( "GCC diagnostic ignored \"-Wall\"" )                       \
            _Pragma( "GCC diagnostic ignored \"-Wextra\"" )                     \
            _Pragma( "GCC diagnostic ignored \"-Wpedantic\"" )                  \
            _Pragma( "GCC diagnostic ignored \"-Wunused-local-typedefs\"" )     \
            _Pragma( "GCC diagnostic ignored \"-Woverloaded-virtual\"" )        \
            _Pragma( "GCC diagnostic ignored \"-Wunused-parameter\"" )          \
            _Pragma( "GCC diagnostic ignored \"-Wdeprecated-copy\"" )           \
            _Pragma( "GCC diagnostic ignored \"-Wdeprecated-declarations\"" )   \
            _Pragma( "GCC diagnostic ignored \"-Wvirtual-move-assign\"" )       \
            _Pragma( "GCC diagnostic ignored \"-Wunused-function\"" )           \
            _Pragma( "GCC diagnostic ignored \"-Woverflow\"" )                  \
            _Pragma( "GCC diagnostic ignored \"-Wunused-variable\"" )           \
            _Pragma( "GCC diagnostic ignored \"-Wignored-qualifiers\"" )        \
            _Pragma( "GCC diagnostic ignored \"-Wenum-compare\"" )              \
            _Pragma( "GCC diagnostic ignored \"-Wsign-compare\"" )              \
            _Pragma( "GCC diagnostic ignored \"-Wterminate\"" )                 \
            _Pragma( "GCC diagnostic ignored \"-Wimplicit-fallthrough\"" )      \
            _Pragma( "GCC diagnostic ignored \"-Wmaybe-uninitialized\"" )
        #define ENABLE_WARNINGS _Pragma( "GCC diagnostic pop" )
    #else
        #define DISABLE_WARNINGS
        #define ENABLE_WARNINGS
    #endif
#endif
// clang-format on
