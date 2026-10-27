#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "probe_library.h"
int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[2], "--compat") &&
        !load_probe_library()) {
        printf("FAIL load compatibility DLL error=%lu\n", GetLastError()); return 9;
    }
    HANDLE d = CreateFileW(L"\\\\.\\voicemodvad", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD bytes = 0;
    unsigned long long status[130] = {0};
    if (d == INVALID_HANDLE_VALUE) {
        printf("FAIL open error=%lu\n", GetLastError()); return 1;
    }
    printf("PASS open control device\n");
    if (!DeviceIoControl(d, 0x1d201c, status, sizeof(status), status,
                         sizeof(status), &bytes, NULL) || bytes != sizeof(status)) {
        printf("FAIL status bytes=%lu error=%lu\n", bytes, GetLastError()); return 2;
    }
    printf("PASS status query bytes=%lu\n", bytes);
    if (argc > 1 && !strcmp(argv[1], "--full")) {
        HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (!DeviceIoControl(d, 0x1d2007, event, sizeof(event), NULL, 0, &bytes, NULL)) {
            printf("FAIL register event error=%lu\n", GetLastError()); return 3;
        }
        if (!DeviceIoControl(d, 0x1d200f, NULL, 0, NULL, 0, &bytes, NULL) ||
            WaitForSingleObject(event, 0) != WAIT_OBJECT_0) {
            printf("FAIL signal event error=%lu\n", GetLastError()); return 4;
        }
        if (!DeviceIoControl(d, 0x1d2013, NULL, 0, NULL, 0, &bytes, NULL) ||
            WaitForSingleObject(event, 0) != WAIT_TIMEOUT) {
            printf("FAIL clear event error=%lu\n", GetLastError()); return 5;
        }
        if (!DeviceIoControl(d, 0x1d200b, NULL, 0, NULL, 0, &bytes, NULL)) return 6;
        CloseHandle(event);
        printf("PASS register/signal/clear/release event\n");
    }
    if (DeviceIoControl(d, 0x1d201c, NULL, 0, status, 1, &bytes, NULL)) return 7;
    printf("PASS malformed output rejected\n");
    if (DeviceIoControl(d, 0x222000, NULL, 0, NULL, 0, &bytes, NULL)) return 8;
    printf("PASS unknown control rejected\n");
    CloseHandle(d);
    return 0;
}
