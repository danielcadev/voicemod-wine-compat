#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

/* Wine's scanf consumes an initial N while trying nan, then returns EOF for
 * identifiers such as NoReverb. Voicemod's XML loader treats a matching failure
 * (0) as a reference and any other result as a literal. Repair only that call.
 * The SDK still looks up, evaluates and links the real parameters itself.
 * No XML, parameter values, account state, or executable files are changed.
 * Call enable_clean_mic_fix() at process attach. Controlled by:
 * VMCOMPAT_FIX_FLOAT_REFS=1. Log: %TEMP%\vmcompat-clean-mic-fix.log.
 */
typedef int (__cdecl *common_scan_fn)(uint64_t,const char *,size_t,const char *,void *,va_list);
static common_scan_fn original_scan;
static uintptr_t image_base;
static HANDLE fix_log=INVALID_HANDLE_VALUE;
static LONG logged_corrections;

static int reference_identifier(const char *text)
{
    if (!text || (*text!='N' && *text!='n')) return 0;
    /* Restrict correction to a nonempty, bounded XML identifier. Never turn
     * an empty-input EOF or an incomplete signed number into a reference. */
    for (unsigned i=0;i<128;++i) {
        unsigned char c=(unsigned char)text[i];
        if (!c) return 1;
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
              (c>='0' && c<='9') || c==' ' || c=='_' || c=='-')) return 0;
    }
    return 0;
}

static int from_xml_reference_scan(void)
{
    void *frames[8];
    USHORT count=CaptureStackBackTrace(0,8,frames,NULL);
    for (USHORT i=0;i<count;++i)
        if ((uintptr_t)frames[i]==image_base+0x928c8) return 1;
    return 0;
}

static void log_line(const char *text)
{
    DWORD written;
    if (fix_log!=INVALID_HANDLE_VALUE)
        WriteFile(fix_log,text,(DWORD)strlen(text),&written,NULL);
}

static int __cdecl compat_scan(uint64_t options,const char *source,size_t length,
    const char *format,void *locale,va_list args)
{
    const void *caller=__builtin_return_address(0);
    int result=original_scan(options,source,length,format,locale,args);
    if (result==-1 && options==2 && length==(size_t)-1 && !locale &&
        (uintptr_t)caller==image_base+0xb19eda && format && !strcmp(format,"%lf") &&
        reference_identifier(source) && from_xml_reference_scan()) {
        if (InterlockedIncrement(&logged_corrections)<=16) {
            /* Log only known public voice identifiers, never arbitrary data. */
            const char *name=!strcmp(source,"NoReverb")?"NoReverb":
                !strcmp(source,"NoSpace")?"NoSpace":
                !strcmp(source,"NaturalVoice")?"NaturalVoice":"N-prefixed parameter";
            char line[160];
            snprintf(line,sizeof line,"XML reference %s: scanf EOF -> matching failure; native parameter linking retained\r\n",name);
            log_line(line);
            if (fix_log!=INVALID_HANDLE_VALUE) FlushFileBuffers(fix_log);
        }
        return 0;
    }
    return result;
}

void enable_clean_mic_fix(void)
{
    char enabled[8];
    if (GetEnvironmentVariableA("VMCOMPAT_FIX_FLOAT_REFS",enabled,sizeof enabled)!=1 || enabled[0]!='1') return;
    unsigned char *base=(unsigned char *)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt=(IMAGE_NT_HEADERS64 *)(base+dos->e_lfanew);
    static const unsigned char loader_call[]={0x48,0x8d,0x15,0x76,0x31,0x4f,0x02,0x4c,0x8d,0x85,0xe8,0x04,0x00,0x00,0xe8,0xc8,0x75,0xa8,0x00};
    static const unsigned char wrapper_call[]={0xff,0x15,0x06,0xa9,0x33,0x00};
    if (nt->FileHeader.TimeDateStamp!=0x6adb566e || nt->OptionalHeader.SizeOfImage!=0x3246000 ||
        memcmp(base+0x928b5,loader_call,sizeof loader_call) ||
        memcmp(base+0xb19ed4,wrapper_call,sizeof wrapper_call)) return;
    char path[MAX_PATH];
    DWORD n=GetTempPathA(sizeof path,path);
    if (!n || n+sizeof("vmcompat-clean-mic-fix.log")>sizeof path) return;
    strcpy(path+n,"vmcompat-clean-mic-fix.log");
    fix_log=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    common_scan_fn *slot=(common_scan_fn *)(base+0xe547e0);
    original_scan=*slot;
    image_base=(uintptr_t)base;
    DWORD old;
    if (!VirtualProtect(slot,sizeof *slot,PAGE_READWRITE,&old)) {log_line("fix: IAT protection failed\r\n");return;}
    *slot=compat_scan;
    DWORD ignored;
    VirtualProtect(slot,sizeof *slot,old,&ignored);
    log_line("fix: installed for Voicemod 3.17.40; XML loader scanf call only\r\n");
}
