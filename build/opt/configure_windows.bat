REM  Initialize Visual Studio
call "D:\Programs\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86_amd64

rmdir /s/q CMakefiles
rd /s/q CMakeCache.txt


cmake                               ^
    -G "NMake Makefiles"            ^
    -D CMAKE_BUILD_TYPE=Release     ^
    -D CXX_STD=11                   ^
    -D USE_MATLAB=1                 ^
        -D MATLAB_DIRECTORY:PATH="D:/Programs/MATLAB/R2015b" ^
        -D USE_MATLAB_LAPACK=0      ^
    -D MPI_DIRECTORY:PATH="D:/Programs/Microsoft MPI" ^
    -D QWT_URL:PATH="E:\nightly_builds\qwt-6.1.2.tar.bz2" ^
    ..\..\src



