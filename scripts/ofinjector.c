#if !defined(_WIN32) || defined(_WIN64)
#error This project can only be compiled as a 32-bit Windows application.
#endif
#include <Windows.h>
// After
#include <processenv.h>
#include <winuser.h>
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' \
version='6.0.0.0' \
processorArchitecture='*' \
publicKeyToken='6595b64144ccf1df' \
language='*'\"")

// Our implementation because no stdlib
#pragma function(memset)
void* __cdecl memset(void* dest, int c, size_t count) {
    char* bytes = (char*)dest;
    while (count--)
        *bytes++ = (char)c;
    return dest;
}

// Entry point
void __stdcall WinMainCRTStartup() {
    UINT exit_code = 1;
    LPWSTR* argv;
    int argc;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 4) {
        LPWSTR processPath = argv[1];
        LPWSTR dllPath = argv[2];
        LPWSTR projectName = argv[3];

        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};

        // 1. Set project name
        SetEnvironmentVariableW(L"OVERFUSION_PROJECT_NAME", projectName);

        // 2. Start our process suspended
        if (CreateProcessW(processPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si,
                           &pi)) {
            // 3. Do the LoadLibrary DLL injection
            size_t dllPathLen = (lstrlenW(dllPath) + 1) * sizeof(WCHAR);

            LPVOID remoteMem =
                VirtualAllocEx(pi.hProcess, NULL, dllPathLen, MEM_COMMIT, PAGE_READWRITE);
            if (remoteMem) {
                WriteProcessMemory(pi.hProcess, remoteMem, dllPath, dllPathLen, NULL);
                LPVOID loadLibraryAddr =
                    (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
                HANDLE hThread =
                    CreateRemoteThread(pi.hProcess, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMem, 0, NULL);

                if (hThread) {
                    WaitForSingleObject(hThread, INFINITE);
                    CloseHandle(hThread);
                    exit_code = 0;
                }
            }
            // 4. Resume process and cleanup
            ResumeThread(pi.hThread);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        MessageBoxW(NULL,
                    L"Usage: \nofinjector.exe \"game.exe\" \"overfusion.dll\" \"project_name\"",
                    L"Information!", MB_ICONINFORMATION);
    }
    LocalFree(argv);
    ExitProcess(exit_code);
}
