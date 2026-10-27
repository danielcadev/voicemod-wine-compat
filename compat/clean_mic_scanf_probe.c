#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

typedef int (__cdecl *common_scan_fn)(uint64_t,const char *,size_t,const char *,void *,va_list);
static common_scan_fn common_scan;
static int scan(uint64_t options,const char *source,const char *format,...)
{
    va_list args;
    va_start(args,format);
    int result=common_scan(options,source,(size_t)-1,format,NULL,args);
    va_end(args);
    return result;
}
int main(void)
{
    HMODULE module=LoadLibraryW(L"ucrtbase.dll");
    common_scan=(common_scan_fn)(void *)GetProcAddress(module,"__stdio_common_vsscanf");
    if (!common_scan) return 2;
    const char *samples[]={"NoReverb","NoSpace","NaturalVoice","BassVolumeDown","Reverb Volume","Mix","nan","NaN","inf","Infinity","0.98","0.02","", " ","nonsense","No", "n", "nanx", "NAN(foo)"};
    for (unsigned j=0;j<2;++j) for (unsigned i=0;i<sizeof samples/sizeof samples[0];++i) {
        uint64_t options=j?2:0;
        double value=12345.0;
        int result=scan(options,samples[i],"%lf",&value);
        printf("options=%llu input=[%s] result=%d value=%.10g\n",(unsigned long long)options,samples[i],result,value);
    }
    return 0;
}
