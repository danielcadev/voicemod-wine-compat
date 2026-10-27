#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <stdlib.h>
#include <string.h>

/* Wine implements session enumeration but not session-created notifications.
 * Retain subscriptions and discover new sessions by polling the real manager. */
typedef struct {
    IAudioSessionManager2 *manager;
    IAudioSessionNotification *notification;
    WCHAR *known[128];
    unsigned count;
} subscription;
static subscription subscriptions[32];
static SRWLOCK lock=SRWLOCK_INIT;
static struct {IAudioSessionNotification *notification; IAudioSessionControl *control;} pending[128];
static unsigned pending_count;

static void inspect_sessions(subscription *sub,BOOL notify)
{
    IAudioSessionEnumerator *enumerator=NULL;
    int count=0;
    if (FAILED(IAudioSessionManager2_GetSessionEnumerator(sub->manager,&enumerator))) return;
    IAudioSessionEnumerator_GetCount(enumerator,&count);
    for (int i=0;i<count;++i) {
        IAudioSessionControl *control=NULL;
        IAudioSessionControl2 *control2=NULL;
        WCHAR *id=NULL;
        if (FAILED(IAudioSessionEnumerator_GetSession(enumerator,i,&control))) continue;
        if (SUCCEEDED(IAudioSessionControl_QueryInterface(control,&IID_IAudioSessionControl2,(void **)&control2))) {
            HRESULT hr=IAudioSessionControl2_GetSessionInstanceIdentifier(control2,&id);
            if (FAILED(hr)) hr=IAudioSessionControl2_GetSessionIdentifier(control2,&id);
            if (SUCCEEDED(hr)&&id) {
                BOOL seen=FALSE;
                for (unsigned j=0;j<sub->count;++j)
                    if (!wcscmp(sub->known[j],id)) {seen=TRUE;break;}
                if (!seen&&sub->count<128) {
                    size_t size=(wcslen(id)+1)*sizeof(WCHAR);
                    WCHAR *copy=malloc(size);
                    if (copy) {
                        memcpy(copy,id,size);sub->known[sub->count++]=copy;
                        if (notify&&pending_count<128) {
                            IAudioSessionNotification_AddRef(sub->notification);
                            IAudioSessionControl_AddRef(control);
                            pending[pending_count].notification=sub->notification;
                            pending[pending_count++].control=control;
                        }
                    }
                }
                CoTaskMemFree(id);
            }
            IAudioSessionControl2_Release(control2);
        }
        IAudioSessionControl_Release(control);
    }
    IAudioSessionEnumerator_Release(enumerator);
}

static HRESULT WINAPI register_notification(IAudioSessionManager2 *manager,IAudioSessionNotification *notification)
{
    if (!notification) return E_POINTER;
    AcquireSRWLockExclusive(&lock);
    for (unsigned i=0;i<32;++i) {
        subscription *sub=&subscriptions[i];
        if (sub->manager==manager&&sub->notification==notification) {
            ReleaseSRWLockExclusive(&lock);return S_OK;
        }
    }
    for (unsigned i=0;i<32;++i) if (!subscriptions[i].manager) {
        subscription *sub=&subscriptions[i];
        sub->manager=manager;sub->notification=notification;sub->count=0;
        IAudioSessionManager2_AddRef(manager);IAudioSessionNotification_AddRef(notification);
        inspect_sessions(sub,FALSE);
        ReleaseSRWLockExclusive(&lock);
        OutputDebugStringA("vmcompat: registered audio session polling subscription\n");
        return S_OK;
    }
    ReleaseSRWLockExclusive(&lock);return E_OUTOFMEMORY;
}

static HRESULT WINAPI unregister_notification(IAudioSessionManager2 *manager,IAudioSessionNotification *notification)
{
    AcquireSRWLockExclusive(&lock);
    for (unsigned i=0;i<32;++i) {
        subscription *sub=&subscriptions[i];
        if (sub->manager==manager&&sub->notification==notification) {
            for (unsigned j=0;j<sub->count;++j) free(sub->known[j]);
            memset(sub,0,sizeof(*sub));
            ReleaseSRWLockExclusive(&lock);
            IAudioSessionNotification_Release(notification);IAudioSessionManager2_Release(manager);
            return S_OK;
        }
    }
    ReleaseSRWLockExclusive(&lock);return S_OK;
}

static DWORD WINAPI notification_worker(void *unused)
{
    IMMDeviceEnumerator *enumerator=NULL;IMMDevice *device=NULL;
    IAudioSessionManager2 *manager=NULL;
    (void)unused;
    if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED))) return 1;
    HRESULT hr=CoCreateInstance(&CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,
                                &IID_IMMDeviceEnumerator,(void **)&enumerator);
    if (SUCCEEDED(hr)) hr=IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator,eRender,eConsole,&device);
    if (SUCCEEDED(hr)) hr=IMMDevice_Activate(device,&IID_IAudioSessionManager2,CLSCTX_ALL,NULL,(void **)&manager);
    if (SUCCEEDED(hr)) {
        void **table=*(void ***)manager;DWORD old;
        if (VirtualProtect(table+6,2*sizeof(void *),PAGE_READWRITE,&old)) {
            table[6]=register_notification;table[7]=unregister_notification;
            VirtualProtect(table+6,2*sizeof(void *),old,&old);
            OutputDebugStringA("vmcompat: audio notification polling installed\n");
        }
        IAudioSessionManager2_Release(manager);
    }
    if (device) IMMDevice_Release(device);
    if (enumerator) IMMDeviceEnumerator_Release(enumerator);
    if (FAILED(hr)) {CoUninitialize();return 2;}
    for (;;) {
        Sleep(200);
        /* Invoke callbacks after releasing the registry lock: callbacks may
         * synchronously unregister themselves. Retain both COM objects. */
        AcquireSRWLockExclusive(&lock);
        for (unsigned i=0;i<32;++i)
            if (subscriptions[i].manager) inspect_sessions(&subscriptions[i],TRUE);
        ReleaseSRWLockExclusive(&lock);
        for (unsigned i=0;i<pending_count;++i) {
            OutputDebugStringA("vmcompat: discovered new Wine audio session\n");
            IAudioSessionNotification_OnSessionCreated(pending[i].notification,pending[i].control);
            IAudioSessionControl_Release(pending[i].control);
            IAudioSessionNotification_Release(pending[i].notification);
        }
        pending_count=0;
    }
}

void start_audio_notifications(void)
{
    HANDLE thread=CreateThread(NULL,0,notification_worker,NULL,0,NULL);
    if (thread) CloseHandle(thread);
}
