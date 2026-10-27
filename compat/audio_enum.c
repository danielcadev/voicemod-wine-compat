#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <stdio.h>
static const PROPERTYKEY friendly={ {0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14 };
int main(void)
{
    IMMDeviceEnumerator *enumerator=NULL;
    HRESULT hr=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if (FAILED(hr)) return 1;
    hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,
                       &IID_IMMDeviceEnumerator,(void **)&enumerator);
    if (FAILED(hr)) {printf("enumerator error %lx\n",hr);return 2;}
    for (int flow=0;flow<2;++flow) {
        IMMDeviceCollection *collection=NULL;UINT count=0;
        hr=IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,flow,3,&collection);
        if (FAILED(hr)) continue;
        IMMDeviceCollection_GetCount(collection,&count);
        for (UINT i=0;i<count;++i) {
            IMMDevice *device=NULL;IPropertyStore *store=NULL;PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(IMMDeviceCollection_Item(collection,i,&device))) {
                if (SUCCEEDED(IMMDevice_OpenPropertyStore(device,STGM_READ,&store))) {
                    hr=IPropertyStore_GetValue(store,&friendly,&value);
                    if (SUCCEEDED(hr)&&value.vt==VT_LPWSTR)
                        wprintf(L"%ls PKEY_Device_FriendlyName=[%ls]\n",flow?L"capture":L"render",value.pwszVal);
                    PropVariantClear(&value);IPropertyStore_Release(store);
                }
                IMMDevice_Release(device);
            }
        }
        IMMDeviceCollection_Release(collection);
    }
    IMMDeviceEnumerator_Release(enumerator);CoUninitialize();return 0;
}
