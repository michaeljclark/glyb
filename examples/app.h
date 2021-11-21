#pragma once

#ifdef _WIN32
#ifdef __glad_h_
#undef APIENTRY
#endif
#include <Windows.h>
#include <assert.h>

// translates command-line arguments from UTF-16 to UTF-8 and calls main
static int _main_trampoline(int (*f)(int, char **))
{
    LPWSTR *wargv;
    char **argv, *p;
    int argc, *argl, argt, r;

    // get the UTF-16 command line and convert it to a vector
    wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) Panic("CommandLineToArgvW failed: error=%d", GetLastError());

    // measure the total UTF-8 size for the command line vector
    argl = (int*)LocalAlloc(LPTR, sizeof(int) * argc);
    argt = 0;
    if (!argl) Panic("LocalAlloc failed: error=%d", GetLastError());
    for (int i = 0; i < argc; i++) {
        argt += (argl[i] = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL));
    }

    // allocate and populate argv array using saved UTF-8 lengths
    argv = (char**)LocalAlloc(LPTR, sizeof(char*) * argc + argt);
    if (!argv) Panic("LocalAlloc failed: error=%d", GetLastError());
    p = (char*)(argv + argc);
    for (int i = 0; i < argc; i++) {
        argv[i] = p;
        p += argl[i];
    }

    // convert command line to UTF-8 using saved UTF-8 offsets and lengths
    for (int i = 0; i < argc; i++) {
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], argl[i], NULL, NULL);
    }

    r = f(argc, argv);

    LocalFree(argl);
    LocalFree(argv);

    return r;
}

#define declare_main(main_func)                             \
INT WINAPI WinMain(_In_ HINSTANCE hInstance,                \
                   _In_opt_ HINSTANCE hPrevInstance,        \
                   _In_ PSTR lpCmdLine,                     \
                   _In_ int nShowCmd)                       \
{                                                           \
    return _main_trampoline(main_func);                     \
}
#else
#define declare_main(main_func)                             \
int main(int argc, char** argv)                             \
{                                                           \
    return main_func(argc, argv);                           \
}
#endif
