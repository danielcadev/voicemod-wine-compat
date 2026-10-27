#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
static const GUID media_class={0x4d36e96c,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
static int property(HDEVINFO set,SP_DEVINFO_DATA *dev,DWORD key,const WCHAR *value)
{
    if (!SetupDiSetDeviceRegistryPropertyW(set,dev,key,(const BYTE *)value,
        (lstrlenW(value)+1)*sizeof(WCHAR))) {
        printf("property %lu error=%lu\n",key,GetLastError());return 0;
    }
    return 1;
}
int main(void)
{
    SP_DEVINFO_DATA dev={sizeof(dev),{0},0,0};
    HDEVINFO set=SetupDiCreateDeviceInfoList(&media_class,NULL);
    if (set==INVALID_HANDLE_VALUE) return 1;
    if (!SetupDiOpenDeviceInfoW(set,L"ROOT\\VMCOMPAT\\0000",NULL,0,&dev)) {
        if (!SetupDiCreateDeviceInfoW(set,L"ROOT\\VMCOMPAT\\0000",&media_class,
             L"Voicemod Wine/PipeWire compatibility bridge",NULL,0,&dev)) {
            printf("create error=%lu\n",GetLastError());return 2;
        }
        const WCHAR hwid[]=L"ROOT\\VMCOMPAT\0";
        if (!SetupDiSetDeviceRegistryPropertyW(set,&dev,SPDRP_HARDWAREID,
              (const BYTE *)hwid,sizeof(hwid)) ||
            !SetupDiRegisterDeviceInfo(set,&dev,0,NULL,NULL,NULL)) {
            printf("register error=%lu\n",GetLastError());return 3;
        }
    }
    if (!property(set,&dev,SPDRP_DEVICEDESC,L"Wine/PipeWire (Voicemod Virtual Audio Device (WDM))") ||
        !property(set,&dev,SPDRP_MFG,L"Wine/PipeWire compatibility experiment") ||
        !property(set,&dev,SPDRP_SERVICE,L"vmcompat")) return 4;
    HKEY key=SetupDiCreateDevRegKeyW(set,&dev,DICS_FLAG_GLOBAL,0,DIREG_DRV,NULL,NULL);
    if (key==INVALID_HANDLE_VALUE) {
        printf("driver key error=%lu\n",GetLastError());return 5;
    }
    /* Advertise the original control ABI version implemented by this bridge,
     * while explicitly identifying its independent compatibility provider. */
    const WCHAR version[]=L"2022.6.1.0";
    const WCHAR provider[]=L"Wine/PipeWire compatibility experiment";
    LSTATUS status=RegSetValueExW(key,L"DriverVersion",0,REG_SZ,(const BYTE *)version,sizeof(version));
    if (!status) status=RegSetValueExW(key,L"ProviderName",0,REG_SZ,(const BYTE *)provider,sizeof(provider));
    RegCloseKey(key);SetupDiDestroyDeviceInfoList(set);
    if (status) {printf("registry error=%ld\n",status);return 6;}
    printf("Registered ROOT\\VMCOMPAT\\0000 with control ABI 2022.6.1.0\n");
    return 0;
}
