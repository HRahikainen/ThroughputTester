@echo off
cls

:: Note: This is the standard installation path for the GSDK, if it's different on your installation please update the variable below
SET SDKS_PATH=C:\SiliconLabs\SimplicityStudio\v4\developer\sdks\gecko_sdk_suite
cd /d %SDKS_PATH%

:: Look for the highest numbered SDK folder and set a variable with it, to ensure we are always using the latest SDK version
for /f "tokens=*" %%1 in ('dir /b /ad /on v*') do (
    set latestdir=%%1
)

SET LATEST_SDK_PATH=%SDKS_PATH%\%latestdir%

:: Back to our project directory
cd %~dp0

:: Make needed directories
mkdir lcd-graphics
mkdir dmd

:: Copy files from the SDK into the project
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\display.c" 					"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\display.h" 					"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displayconfigall.h" 		"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\emstatus.h"			 		"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displayls013b7dh03.c" 		"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displayls013b7dh03.h" 		"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displaypalemlib.c" 			"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displaybackend.h" 			"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\displaypal.h" 				"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\udelay.c" 					"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\textdisplay.h" 				"lcd-graphics"
copy "%LATEST_SDK_PATH%\hardware\kit\common\drivers\retargettextdisplay.h" 		"lcd-graphics"

copy "%LATEST_SDK_PATH%\util\silicon_labs\silabs_core\graphics\graphics.c" 				"lcd-graphics"
copy "%LATEST_SDK_PATH%\util\silicon_labs\silabs_core\graphics\graphics.h" 				"lcd-graphics"
copy "%LATEST_SDK_PATH%\util\silicon_labs\silabs_core\graphics\displayconfigapp.h" 		"lcd-graphics"

copy "%LATEST_SDK_PATH%\platform\middleware\glib\glib\*" 						"lcd-graphics"
copy "%LATEST_SDK_PATH%\platform\middleware\glib\dmd\dmd.h" 					"lcd-graphics"
copy "%LATEST_SDK_PATH%\platform\middleware\glib\dmd\display\dmd_display.c" 	"lcd-graphics"
copy "%LATEST_SDK_PATH%\platform\middleware\glib\dmd\dmd.h" 					"dmd"
copy "%LATEST_SDK_PATH%\platform\middleware\glib\bmp_conf.h" 					"lcd-graphics"
copy "%LATEST_SDK_PATH%\platform\middleware\glib\em_types.h" 					"lcd-graphics"

copy %LATEST_SDK_PATH%\platform\emdrv\gpiointerrupt\src\gpiointerrupt.c" ""