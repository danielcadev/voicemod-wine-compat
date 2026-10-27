#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Diagnostic only. Logs only the exact public Clean Mic control label.
 * Does not change parser inputs, return values, licenses, or authentication.
 * Call enable_clean_mic_probe() at process attach, before application threads.
 * Opt in with VMCOMPAT_TRACE_CLEAN=1. Output: %TEMP%\vmcompat-clean-mic.log.
 * Verified artifact: Voicemod 3.17.40, SHA256 f62420ebb2452c33e0948b0cb175a44438e32134af2b332bc4c302b7d9497805.
 */
typedef void (__cdecl *split_fn)(void *, const void *, unsigned char);
static split_fn original_split;
static HANDLE trace_file = INVALID_HANDLE_VALUE;
static LONG trace_count;

static int readable(const void *pointer, size_t length)
{
    MEMORY_BASIC_INFORMATION m;
    uintptr_t p=(uintptr_t)pointer;
    if (!p || p+length<p || !VirtualQuery(pointer,&m,sizeof m)) return 0;
    return m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
        p+length <= (uintptr_t)m.BaseAddress+m.RegionSize;
}

static const char *bounded_string(const void *object, size_t *length)
{
    if (!readable(object,32)) return NULL;
    const uintptr_t *s=object;
    size_t n=s[2], cap=s[3];
    if (!n || n>128 || cap<n || cap>65535) return NULL;
    const char *p=cap<16?(const char *)object:(const char *)s[0];
    if (!readable(p,n+1) || p[n]) return NULL;
    for (size_t i=0;i<n;++i) if ((unsigned char)p[i]<32 || (unsigned char)p[i]>126) return NULL;
    *length=n;
    return p;
}

static void emit(const char *line)
{
    DWORD written;
    if (trace_file!=INVALID_HANDLE_VALUE)
        WriteFile(trace_file,line,(DWORD)strlen(line),&written,NULL);
}

static void __cdecl traced_split(void *parameter,const void *value,unsigned char separator)
{
    size_t length=0;
    const char *text=bounded_string(value,&length);
    if (text && ((length==strlen("NoReverb,Reverb Volume") &&
        !memcmp(text,"NoReverb,Reverb Volume",length)) ||
        (length==strlen("NoReverb,ReverbVolume") &&
        !memcmp(text,"NoReverb,ReverbVolume",length))) &&
        InterlockedIncrement(&trace_count)<=4) {
        char line[512];
        snprintf(line,sizeof line,"Clean Mic label parsed as float list; parameter=%p separator=%02x\r\n",parameter,separator);
        emit(line);
        /* Parameter objects hold their name/description in MSVC string fields.
         * Inspect bounded fields only; do not dump process or XML buffers. */
        for (unsigned offset=0;offset<=0x90;offset+=8) {
            size_t n=0;
            const char *s=bounded_string((const char *)parameter+offset,&n);
            if (s) {
                snprintf(line,sizeof line,"parameter.string+%02x=%.*s\r\n",offset,(int)n,s);
                emit(line);
            }
        }
        void *frames[24];
        USHORT count=CaptureStackBackTrace(0,24,frames,NULL);
        uintptr_t base=(uintptr_t)GetModuleHandleW(NULL);
        for (USHORT i=0;i<count;++i) {
            uintptr_t address=(uintptr_t)frames[i];
            if (address>=base && address<base+0x3246000)
                snprintf(line,sizeof line,"frame[%u]=Voicemod.exe+0x%llx\r\n",i,(unsigned long long)(address-base));
            else snprintf(line,sizeof line,"frame[%u]=%p\r\n",i,frames[i]);
            emit(line);
        }
        FlushFileBuffers(trace_file);
    }
    original_split(parameter,value,separator);
}

static void absolute_jump(unsigned char *where,const void *target)
{
    where[0]=0xff;where[1]=0x25;
    memset(where+2,0,4);
    memcpy(where+6,&target,8);
}

void enable_clean_mic_probe(void)
{
    char enabled[8];
    if (!GetEnvironmentVariableA("VMCOMPAT_TRACE_CLEAN",enabled,sizeof enabled) || strcmp(enabled,"1")) return;
    unsigned char *base=(unsigned char *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(base+dos->e_lfanew);
    static const unsigned char expected[]={0x55,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x56,0x57,0x53,0x48,0x81,0xec,0x88,0x01,0x00,0x00};
    unsigned char *entry=base+0x217750;
    if (nt->FileHeader.TimeDateStamp!=0x6adb566e || nt->OptionalHeader.SizeOfImage!=0x3246000 ||
        memcmp(entry,expected,sizeof expected)) return;
    char path[MAX_PATH];
    DWORD n=GetTempPathA(sizeof path,path);
    if (!n || n+sizeof("vmcompat-clean-mic.log")>sizeof path) return;
    strcpy(path+n,"vmcompat-clean-mic.log");
    trace_file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (trace_file==INVALID_HANDLE_VALUE) return;
    unsigned char *trampoline=VirtualAlloc(NULL,64,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!trampoline) {emit("probe: trampoline allocation failed\r\n");return;}
    memcpy(trampoline,expected,sizeof expected);
    absolute_jump(trampoline+sizeof expected,entry+sizeof expected);
    DWORD old;
    if (!VirtualProtect(trampoline,64,PAGE_EXECUTE_READ,&old)) {emit("probe: trampoline protection failed\r\n");VirtualFree(trampoline,0,MEM_RELEASE);return;}
    original_split=(split_fn)trampoline;
    if (!VirtualProtect(entry,sizeof expected,PAGE_EXECUTE_READWRITE,&old)) {emit("probe: entry protection failed\r\n");VirtualFree(trampoline,0,MEM_RELEASE);return;}
    absolute_jump(entry,(const void *)traced_split);
    memset(entry+14,0x90,sizeof expected-14);
    DWORD ignored;
    VirtualProtect(entry,sizeof expected,old,&ignored);
    FlushInstructionCache(GetCurrentProcess(),entry,sizeof expected);
    emit("probe: installed for Voicemod 3.17.40; original parsing retained\r\n");
}
