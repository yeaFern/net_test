@ECHO OFF

RMDIR /Q /S build
MKDIR build
PUSHD build

cmake .. -G "Visual Studio 16"
REM cmake --build . --config Debug
POPD
