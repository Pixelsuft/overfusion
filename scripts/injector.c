#include <Windows.h>
// After
#include <processenv.h>

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
    // Assuming: injector.exe "game.exe" "overfusion.dll" "project_name"
    if (argc >= 4) {
        LPWSTR processPath = argv[1];
        LPWSTR dllPath = argv[2];
        LPWSTR projectName = argv[3];

        STARTUPINFOW si = {sizeof(si)};
        PROCESS_INFORMATION pi = {0};

        // Set project name
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
    }
    LocalFree(argv);
    ExitProcess(exit_code);
}
