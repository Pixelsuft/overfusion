@echo off
cl.exe /nologo /O2 /GS- /W3 /TC /c ofinjector.c
link.exe /nologo /NODEFAULTLIB /ENTRY:WinMainCRTStartup /SUBSYSTEM:CONSOLE ^
         kernel32.lib shell32.lib user32.lib ofinjector.obj /OUT:ofinjector.exe
del ofinjector.obj
echo Done!
