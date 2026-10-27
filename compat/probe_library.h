#include <wchar.h>

/* Resolve our DLL beside the probe, independent of checkout and user names. */
static HMODULE load_probe_library(void)
{
    WCHAR path[32768];
    DWORD n=GetModuleFileNameW(NULL,path,sizeof(path)/sizeof(path[0]));
    if (!n || n>=sizeof(path)/sizeof(path[0])) return NULL;
    WCHAR *last=wcsrchr(path,L'\\');
    if (!last || (size_t)(last-path)+1+wcslen(L"version.dll")>=sizeof(path)/sizeof(path[0])) return NULL;
    wcscpy(last+1,L"version.dll");
    return LoadLibraryW(path);
}
