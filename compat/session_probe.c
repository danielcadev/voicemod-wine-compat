#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <audioclient.h>
#include <stdio.h>
#include "probe_library.h"

static LONG refs=1;
static HANDLE created;
static HRESULT WINAPI query(IAudioSessionNotification *self,REFIID iid,void **out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualIID(iid,&IID_IUnknown)&&!IsEqualIID(iid,&IID_IAudioSessionNotification)) return E_NOINTERFACE;
    *out=self;InterlockedIncrement(&refs);return S_OK;
}
static ULONG WINAPI retain(IAudioSessionNotification *self) {(void)self;return InterlockedIncrement(&refs);}
static ULONG WINAPI release(IAudioSessionNotification *self) {(void)self;return InterlockedDecrement(&refs);}
static HRESULT WINAPI on_created(IAudioSessionNotification *self,IAudioSessionControl *session)
{(void)self;(void)session;SetEvent(created);return S_OK;}
static IAudioSessionNotificationVtbl table={query,retain,release,on_created};
static IAudioSessionNotification notification={&table};
#define CHECK(call) do { HRESULT result=(call); if (FAILED(result)) {printf("FAIL %s: %08lx\n",#call,result);return 1;} } while(0)
int main(void)
{
    IMMDeviceEnumerator *enumerator=NULL;IMMDevice *device=NULL;
    IAudioSessionManager2 *manager=NULL;IAudioClient *client=NULL;
    WAVEFORMATEX *format=NULL;GUID session;
    CHECK(CoInitializeEx(NULL,COINIT_MULTITHREADED));
    if (!load_probe_library()) return 2;
    Sleep(500);
    created=CreateEventW(NULL,FALSE,FALSE,NULL);
    CHECK(CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,&IID_IMMDeviceEnumerator,(void **)&enumerator));
    CHECK(IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator,eRender,eConsole,&device));
    CHECK(IMMDevice_Activate(device,&IID_IAudioSessionManager2,CLSCTX_ALL,NULL,(void **)&manager));
    CHECK(IAudioSessionManager2_RegisterSessionNotification(manager,&notification));
    CHECK(CoCreateGuid(&session));
    CHECK(IMMDevice_Activate(device,&IID_IAudioClient,CLSCTX_ALL,NULL,(void **)&client));
    CHECK(IAudioClient_GetMixFormat(client,&format));
    CHECK(IAudioClient_Initialize(client,AUDCLNT_SHAREMODE_SHARED,0,1000000,0,format,&session));
    DWORD result=WaitForSingleObject(created,3000);
    CHECK(IAudioSessionManager2_UnregisterSessionNotification(manager,&notification));
    IAudioClient_Release(client);CoTaskMemFree(format);
    IAudioSessionManager2_Release(manager);IMMDevice_Release(device);IMMDeviceEnumerator_Release(enumerator);
    CloseHandle(created);CoUninitialize();
    if (result!=WAIT_OBJECT_0) {puts("FAIL new session callback missing");return 3;}
    puts("PASS real new audio session triggered notification callback");
    return 0;
}
