The GUI currently requires QT and QWT.  When building the GUI the following versions have been tested:
    C++17 / QT4 / QWT 6.1.3
    C++17 / QT4 / QWT 6.3.0
    c++20 / qt5 / QWT 6.3.0
    c++23 / qt5 / QWT 6.3.0
Note that C++20 requires Qt5



Example configure script:

cmake                                                   \
    -G "Unix Makefiles"                                 \
    -D CMAKE_BUILD_TYPE=Release                         \
    -D CMAKE_C_COMPILER:PATH=mpicc                      \
        -D CFLAGS="-fPIC"                               \
    -D CMAKE_CXX_COMPILER:PATH=mpic++                   \
        -D CXXFLAGS="-fPIC"                             \
    -D CMAKE_Fortran_COMPILER:PATH=gfortran             \
    -D USE_MPI=1                                        \
    -D QT_VERSION=4                                     \
        -D QWT_URL=/packages/archive/qwt-6.3.0.tar.bz2  \
    ../../src


