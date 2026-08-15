# Help Me !

## Start Devel

### Deps
C++
Qt6: Core, Widgets, Sql
Sqlite


### MinGW64
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6 mingw-w64-x86_64-cmake
qmake6.exe "CONFIG+=debug" ACTA_Gamma.pro -o Makefile
make 

### Linux
Easy peasy


### Tests

cd tests
qmake tests.pro -o Makefile
make
./release/test_contextdao.exe
