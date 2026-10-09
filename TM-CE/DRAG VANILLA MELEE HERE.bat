@echo off
setlocal

REM set directory to location of this file. Needed when drag n dropping across directories.
cd /d %~dp0

if not exist release-config.bat (
    echo ERROR: Missing release-config.bat. Download the complete Tyro release archive.
    goto end
)
call release-config.bat

set ISO="%~1"

if %ISO%=="" (
    echo Please drag a v1.02 NTSC melee iso on top of 'DRAG VANILLA MELEE HERE.bat'
    goto end
) else (
    echo iso: %ISO%
)

xdelta3 -f -d -s %ISO% patch.xdelta "%OUTPUT_ISO%" || ( echo ERROR: The ISO is not a valid source Melee iso & goto end )
echo %OUTPUT_ISO% has been successfully created!

:end

REM pause if not run from command line
echo %CMDCMDLINE% | findstr /C:"/c">nul && pause
