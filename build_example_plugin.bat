@echo off
echo Building example_plugin...

set PLUGIN_DIR=example_plugin
set BUILD_DIR=%PLUGIN_DIR%\build

echo Creating build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Running CMake...
cd %BUILD_DIR%
cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

echo Running Ninja...
ninja

echo Done!
cd ..\..
