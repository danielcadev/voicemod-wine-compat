#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

/* Wine's per-process Core Audio sessions cannot represent native Discord's
 * capture session. Opt into continuous capture, keeping monitoring independent.
 * This is an in-memory change restricted to the inspected Voicemod build. */
void enable_continuous_capture(void)
{
    char option[8];
    if (!GetEnvironmentVariableA("VMCOMPAT_CAPTURE_ALWAYS",option,sizeof(option)) || strcmp(option,"1")) return;
    BYTE *base=(BYTE *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS *)(base+dos->e_lfanew);
    static const BYTE expected[]={
        0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x57,0x48,0x83,0xec,0x50,
        0x48,0x8b,0xf9,0x0f,0xb6,0xda,0x48,0x81,0xc1,0xa8,0x05,0x00,0x00,
        0xe8,0x0f,0xc7,0x00,0x00,0x3a,0xc3,0x75,0x78,0x80,0xfb,0x01
    };
    static const BYTE initial_expected[]={0x41,0x0f,0xb6,0x47,0x70,0x88,0x83,0xc8,0x02,0x00,0x00,0x4c,0x89,0xb3,0xd0,0x02,0x00,0x00};
    if (nt->FileHeader.TimeDateStamp!=0x6adb566e || nt->OptionalHeader.SizeOfImage!=0x3246000 ||
        memcmp(base+0xb69ff0,expected,sizeof(expected)) ||
        memcmp(base+0xb72488,initial_expected,sizeof(initial_expected))) {
        OutputDebugStringA("vmcompat: continuous capture unsupported for this executable\n");return;
    }
    BYTE *target=base+0xb6a002;
    static const BYTE always_active[]={0x31,0xdb,0x90}; /* kCaptureMode=0: xor ebx,ebx; nop */
    DWORD old;
    if (!VirtualProtect(target,sizeof(always_active),PAGE_EXECUTE_READWRITE,&old)) return;
    memcpy(target,always_active,sizeof(always_active));
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(always_active));
    VirtualProtect(target,sizeof(always_active),old,&old);
    /* The engine constructor sets its initial topology without using the
     * selector. Apply the same capture policy there before any stream starts. */
    target=base+0xb72488;
    static const BYTE initial_capture[]={0x31,0xc0,0x90,0x90,0x90};
    if (!VirtualProtect(target,sizeof(initial_capture),PAGE_EXECUTE_READWRITE,&old)) return;
    memcpy(target,initial_capture,sizeof(initial_capture));
    FlushInstructionCache(GetCurrentProcess(),target,sizeof(initial_capture));
    VirtualProtect(target,sizeof(initial_capture),old,&old);
    OutputDebugStringA("vmcompat: continuous capture enabled; monitoring remains independent\n");
}
