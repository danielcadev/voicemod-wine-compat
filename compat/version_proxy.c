#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

/* Keep Voicemod's signed executable intact. Preserve an explicitly supplied
 * Qt software-rendering option that the application otherwise clears. */
typedef unsigned char (__cdecl *unsetenv_fn)(const char *);
static unsetenv_fn original_unsetenv;
static HMODULE real_version;
extern void start_audio_notifications(void);
extern void enable_continuous_capture(void);
extern void enable_clean_mic_probe(void);
extern void enable_clean_mic_fix(void);
static HANDLE (WINAPI *original_create_w)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static HANDLE (WINAPI *original_create_a)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static BOOL (WINAPI *original_ioctl)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL (WINAPI *original_close)(HANDLE);
static SRWLOCK device_lock = SRWLOCK_INIT;
static struct { HANDLE device, event; } devices[32];

static void track_device(HANDLE handle)
{
    if (handle == INVALID_HANDLE_VALUE) return;
    AcquireSRWLockExclusive(&device_lock);
    for (unsigned i=0; i<32; ++i) if (!devices[i].device) {
        devices[i].device=handle; devices[i].event=NULL; break;
    }
    ReleaseSRWLockExclusive(&device_lock);
}

static HANDLE WINAPI compat_create_w(LPCWSTR name,DWORD access,DWORD share,
    LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template)
{
    HANDLE h=original_create_w(name,access,share,security,creation,flags,template);
    if (name && (!_wcsicmp(name,L"\\\\.\\VMDriver") || !_wcsicmp(name,L"\\\\.\\voicemodvad")))
        track_device(h);
    return h;
}

static HANDLE WINAPI compat_create_a(LPCSTR name,DWORD access,DWORD share,
    LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template)
{
    HANDLE h=original_create_a(name,access,share,security,creation,flags,template);
    if (name && (!_stricmp(name,"\\\\.\\VMDriver") || !_stricmp(name,"\\\\.\\voicemodvad")))
        track_device(h);
    return h;
}

static BOOL WINAPI compat_close(HANDLE handle)
{
    AcquireSRWLockExclusive(&device_lock);
    for (unsigned i=0; i<32; ++i) if (devices[i].device==handle) {
        if (devices[i].event) original_close(devices[i].event);
        devices[i].device=NULL; devices[i].event=NULL;
    }
    ReleaseSRWLockExclusive(&device_lock);
    return original_close(handle);
}

static BOOL WINAPI compat_ioctl(HANDLE device,DWORD code,LPVOID input,DWORD input_size,
    LPVOID output,DWORD output_size,LPDWORD bytes,LPOVERLAPPED overlapped)
{
    /* The original ABI passes an event HANDLE as the METHOD_NEITHER pointer.
     * Wine marshals that pointer as memory and fails with ERROR_NOACCESS before
     * dispatch. Keep that event in the client process; other requests still go
     * to the control device. */
    if (code==0x1d2007 || code==0x1d200b || code==0x1d200f || code==0x1d2013) {
        AcquireSRWLockExclusive(&device_lock);
        for (unsigned i=0; i<32; ++i) if (devices[i].device==device) {
            BOOL ok=TRUE;
            if (overlapped || !bytes || output_size ||
                (code==0x1d2007 ? input_size<sizeof(HANDLE) : input_size!=0)) {
                SetLastError(ERROR_INVALID_PARAMETER); ok=FALSE;
            } else if (code==0x1d2007) {
                HANDLE event=NULL;
                ok=DuplicateHandle(GetCurrentProcess(),(HANDLE)input,
                    GetCurrentProcess(),&event,0,FALSE,DUPLICATE_SAME_ACCESS);
                if (ok) {
                    if (devices[i].event) original_close(devices[i].event);
                    devices[i].event=event;
                    OutputDebugStringA("vmcompat: registered client event locally\n");
                }
            } else if (code==0x1d200b) {
                if (devices[i].event) original_close(devices[i].event);
                devices[i].event=NULL;
            } else if (devices[i].event) {
                ok=code==0x1d200f ? SetEvent(devices[i].event) : ResetEvent(devices[i].event);
            } else { SetLastError(ERROR_INVALID_HANDLE); ok=FALSE; }
            if (bytes) *bytes=0;
            ReleaseSRWLockExclusive(&device_lock);
            return ok;
        }
        ReleaseSRWLockExclusive(&device_lock);
    }
    return original_ioctl(device,code,input,input_size,output,output_size,bytes,overlapped);
}

static unsigned char __cdecl compat_unsetenv(const char *name)
{
    if (name && !strcmp(name, "QTWEBENGINE_CHROMIUM_FLAGS")) {
        char value[2048];
        if (GetEnvironmentVariableA(name, value, sizeof(value)) &&
            strstr(value, "--disable-gpu")) {
            OutputDebugStringA("vmcompat: preserving Qt software rendering flags\n");
            return 1;
        }
    }
    return original_unsetenv ? original_unsetenv(name) : 0;
}

static void patch_qt_environment(void)
{
    BYTE *base = (BYTE *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!rva) return;
    IMAGE_IMPORT_DESCRIPTOR *desc = (IMAGE_IMPORT_DESCRIPTOR *)(base + rva);
    for (; desc->Name; ++desc) {
        if (!desc->OriginalFirstThunk) continue;
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + desc->OriginalFirstThunk);
        IMAGE_THUNK_DATA *slots = (IMAGE_THUNK_DATA *)(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            IMAGE_IMPORT_BY_NAME *name = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            void *replacement=NULL;
            void **original=NULL;
            const char *symbol=(char *)name->Name;
            if (!strcmp(symbol,"?qunsetenv@@YA_NPEBD@Z")) {
                replacement=compat_unsetenv; original=(void **)&original_unsetenv;
            } else if (!strcmp(symbol,"CreateFileW")) {
                replacement=compat_create_w; original=(void **)&original_create_w;
            } else if (!strcmp(symbol,"CreateFileA")) {
                replacement=compat_create_a; original=(void **)&original_create_a;
            } else if (!strcmp(symbol,"DeviceIoControl")) {
                replacement=compat_ioctl; original=(void **)&original_ioctl;
            } else if (!strcmp(symbol,"CloseHandle")) {
                replacement=compat_close; original=(void **)&original_close;
            }
            if (!replacement) continue;
            DWORD old;
            if (VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), PAGE_READWRITE, &old)) {
                *original = (void *)(ULONG_PTR)slots->u1.Function;
                slots->u1.Function = (ULONG_PTR)replacement;
                VirtualProtect(&slots->u1.Function, sizeof(slots->u1.Function), old, &old);
            }
        }
    }
}

static FARPROC version_proc(const char *name)
{
    if (!real_version) {
        WCHAR path[MAX_PATH];
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (!n || n + 13 >= MAX_PATH) return NULL;
        lstrcatW(path, L"\\version.dll");
        real_version = LoadLibraryW(path);
    }
    return real_version ? GetProcAddress(real_version, name) : NULL;
}

DWORD WINAPI proxy_GetFileVersionInfoSizeW(LPCWSTR path, LPDWORD handle)
{
    typedef DWORD (WINAPI *fn)(LPCWSTR, LPDWORD);
    fn f = (fn)version_proc("GetFileVersionInfoSizeW");
    return f ? f(path, handle) : 0;
}

BOOL WINAPI proxy_GetFileVersionInfoW(LPCWSTR path, DWORD handle, DWORD len, LPVOID data)
{
    typedef BOOL (WINAPI *fn)(LPCWSTR, DWORD, DWORD, LPVOID);
    fn f = (fn)version_proc("GetFileVersionInfoW");
    return f ? f(path, handle, len, data) : FALSE;
}

BOOL WINAPI proxy_VerQueryValueW(LPCVOID data, LPCWSTR block, LPVOID *value, PUINT len)
{
    typedef BOOL (WINAPI *fn)(LPCVOID, LPCWSTR, LPVOID *, PUINT);
    fn f = (fn)version_proc("VerQueryValueW");
    return f ? f(data, block, value, len) : FALSE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        patch_qt_environment();
        enable_continuous_capture();
        enable_clean_mic_probe();
        enable_clean_mic_fix();
        start_audio_notifications();
    }
    return TRUE;
}
