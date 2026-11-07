@echo off
setlocal
cd /d "%~dp0"
rem Run in x86 Native Tools Command Prompt for Visual Studio.
rem Embedded shader headers are included. After HLSL edits run python tools\embed_shaders.py.
cl /nologo /std:c++17 /O2 /EHsc /MT /LD src\plugin.cpp /link /MACHINE:X86 /OUT:PSXFX.SA.asi advapi32.lib user32.lib
exit /b %errorlevel%
