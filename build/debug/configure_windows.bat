REM  Initialize Visual Studio
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x86_amd64

rmdir /s/q CMakefiles
rd /s/q CMakeCache.txt


cmake                               ^
    -G "Visual Studio 12 Win64"     ^
    -D CMAKE_BUILD_TYPE=Debug       ^
    -D USE_MPI=1                    ^
    -D USE_MATLAB=0                 ^
        -D MATLAB_DIRECTORY:PATH="C:\Program Files\MATLAB\R2013a" ^
        -D USE_MATLAB_LAPACK=0      ^
    ..\..\src


REM  "NMake Makefiles"   "Visual Studio 12 Win64" 

