@echo off
cl.exe /nologo /O2 /GS- /W3 /TC /c injector.c
link.exe /nologo /NODEFAULTLIB /ENTRY:WinMainCRTStartup /SUBSYSTEM:CONSOLE ^
         kernel32.lib shell32.lib user32.lib injector.obj /OUT:injector.exe
del injector.obj
echo Done!
