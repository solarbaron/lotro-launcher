/*
 * run_patch_client.c - Wrapper to call PatchClient.dll Patch function
 *
 * Based on OneLauncher's implementation.
 * The Patch function uses rundll32 style signature.
 *
 * Compile with MinGW (32-bit required for 32-bit PatchClient.dll):
 *   i686-w64-mingw32-gcc -o run_patch_client.exe run_patch_client.c -static
 *
 * Usage:
 *   run_patch_client.exe "C:\path\to\patchclient.dll" "server:port --language English --filesonly"
 */

#include <stdio.h>
#include <windows.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: <patchclient.dll path> \"<args for patchclient.dll>\"\n");
        return 1;
    }

    printf("Loading DLL: %s\n", argv[1]);
    printf("Arguments: %s\n", argv[2]);
    fflush(stdout);

    HMODULE lib = LoadLibrary(argv[1]);
    if (lib == NULL) {
        fprintf(stderr, "Failed to load patch client DLL. Error: %lu\n", GetLastError());
        return 1;
    }

    printf("DLL loaded successfully\n");
    fflush(stdout);

    // Patch/PatchW use rundll32 style function signatures.
    // The first two arguments aren't relevant to our usage.
    typedef void (*PatchFunc)(void*, void*, const char*);
    PatchFunc Patch = (PatchFunc)GetProcAddress(lib, "Patch");
    if (Patch == NULL) {
        fprintf(stderr, "No `Patch` function found in patch client DLL\n");
        FreeLibrary(lib);
        return 1;
    }

    printf("Calling Patch function...\n");
    fflush(stdout);

    Patch(NULL, NULL, argv[2]);

    printf("Patch function returned\n");
    fflush(stdout);

    // Free the DLL module
    FreeLibrary(lib);
    return 0;
}
