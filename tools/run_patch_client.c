/*
 * run_patch_client.c - Wrapper to call PatchClient.dll Patch function
 * 
 * The Patch function follows rundll32 calling convention:
 *   void CALLBACK Patch(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow);
 * 
 * Compile with MinGW:
 *   i686-w64-mingw32-gcc -o run_patch_client.exe run_patch_client.c -mwindows
 * 
 * Usage:
 *   run_patch_client.exe "C:\path\to\patchclient.dll" "server:port --language English --filesonly"
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

// Rundll32 function signature
typedef void (CALLBACK *PatchFunc)(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow);

// Log file for progress tracking (read by launcher)
static FILE* g_logFile = NULL;

void log_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (g_logFile) {
        vfprintf(g_logFile, format, args);
        fprintf(g_logFile, "\n");
        fflush(g_logFile);
    }
    
    va_end(args);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line (lpCmdLine contains everything after the executable name)
    // Format: "path\to\PatchClient.dll" server:port --language English --filesonly
    
    // Open log file in temp directory
    char logPath[MAX_PATH];
    GetTempPathA(MAX_PATH, logPath);
    strcat(logPath, "lotro_patch_progress.log");
    
    g_logFile = fopen(logPath, "w");
    log_message("PROGRESS:START");
    
    // Use GetCommandLineA to get the full command line
    char* cmdLine = GetCommandLineA();
    log_message("Command: %s", cmdLine);
    
    // Skip the executable name (may be quoted)
    char* args = cmdLine;
    if (*args == '"') {
        args++;
        while (*args && *args != '"') args++;
        if (*args == '"') args++;
    } else {
        while (*args && *args != ' ') args++;
    }
    while (*args == ' ') args++;
    
    // Now args points to: "path\to\PatchClient.dll" server:port --language English --filesonly
    // Or: path\to\PatchClient.dll server:port --language English --filesonly
    
    char dll_path[MAX_PATH] = {0};
    char patch_args[4096] = {0};
    
    // Parse DLL path (may be quoted)
    int i = 0;
    if (*args == '"') {
        args++;
        while (*args && *args != '"' && i < MAX_PATH - 1) {
            dll_path[i++] = *args++;
        }
        if (*args == '"') args++;
    } else {
        while (*args && *args != ' ' && i < MAX_PATH - 1) {
            dll_path[i++] = *args++;
        }
    }
    while (*args == ' ') args++;
    
    // Copy remaining args
    strncpy(patch_args, args, sizeof(patch_args) - 1);
    
    log_message("DLL: %s", dll_path);
    log_message("Args: %s", patch_args);
    
    if (dll_path[0] == '\0') {
        log_message("ERROR:No DLL path specified");
        log_message("PROGRESS:ERROR");
        if (g_logFile) fclose(g_logFile);
        return 1;
    }
    
    // Load the DLL
    log_message("PROGRESS:LOADING");
    HMODULE hDll = LoadLibraryA(dll_path);
    if (!hDll) {
        log_message("ERROR:Failed to load DLL. Error: %lu", GetLastError());
        log_message("PROGRESS:ERROR");
        if (g_logFile) fclose(g_logFile);
        return 2;
    }

    log_message("DLL loaded at: 0x%p", hDll);

    // Get the Patch function
    PatchFunc patchFn = (PatchFunc)GetProcAddress(hDll, "Patch");
    if (!patchFn) {
        log_message("ERROR:Failed to find Patch function. Error: %lu", GetLastError());
        log_message("PROGRESS:ERROR");
        FreeLibrary(hDll);
        if (g_logFile) fclose(g_logFile);
        return 3;
    }

    log_message("Found Patch function");
    log_message("PROGRESS:PATCHING");

    // Call the Patch function with rundll32 convention
    patchFn(NULL, hDll, patch_args, SW_HIDE);

    log_message("Patch returned");
    log_message("PROGRESS:COMPLETE");

    FreeLibrary(hDll);
    if (g_logFile) fclose(g_logFile);
    return 0;
}
