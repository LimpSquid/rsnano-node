@echo off
set exit_code=0

echo "BUILD TYPE %BUILD_TYPE%"
echo "RUN %RUN%"

cmake .. ^
  -Ax64 ^
  %NANO_TEST% ^
  %CI% ^
  -DPORTABLE=1 ^
  -DQt5_DIR="c:\qt\5.13.1\msvc2017_64\lib\cmake\Qt5" ^
  -DNANO_GUI=ON ^
  -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
  -DACTIVE_NETWORK=nano_%NETWORK_CFG%_network ^
  -DNANO_SIMD_OPTIMIZATIONS=TRUE ^
  -Dgtest_force_shared_crt=on ^
  -DBoost_NO_SYSTEM_PATHS=TRUE ^
  -DBoost_NO_BOOST_CMAKE=TRUE ^
  -DNANO_SHARED_BOOST=%NANO_SHARED_BOOST%

set exit_code=%errorlevel%
if %exit_code% neq 0 goto exit

:exit
exit /B %exit_code%