#include <ddk/ntddk.h>

/* Wine-only control-device experiment. PCM is routed through PipeWire, not
 * through this driver. Unknown control requests fail rather than fake success. */
static PDEVICE_OBJECT control_device;
static PKEVENT client_event;

static NTSTATUS finish(PIRP irp, NTSTATUS status, ULONG_PTR bytes)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code;
    NTSTATUS status;
    (void)device;
    if (stack->MajorFunction != IRP_MJ_DEVICE_CONTROL)
        return finish(irp, STATUS_SUCCESS, 0);
    code = stack->Parameters.DeviceIoControl.IoControlCode;
    DbgPrint("vmcompat: ioctl=%08lx input=%lu output=%lu\n", code,
             stack->Parameters.DeviceIoControl.InputBufferLength,
             stack->Parameters.DeviceIoControl.OutputBufferLength);
    switch (code) {
    case 0x1d2007:
        if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HANDLE))
            return finish(irp, STATUS_INVALID_PARAMETER, 0);
        if (client_event) {
            ObDereferenceObject(client_event);
            client_event = NULL;
        }
        status = ObReferenceObjectByHandle(
            (HANDLE)stack->Parameters.DeviceIoControl.Type3InputBuffer,
            EVENT_MODIFY_STATE, *ExEventObjectType, irp->RequestorMode,
            (PVOID *)&client_event, NULL);
        return finish(irp, status, 0);
    case 0x1d200b:
        if (client_event) ObDereferenceObject(client_event);
        client_event = NULL;
        return finish(irp, STATUS_SUCCESS, 0);
    case 0x1d200f:
        if (client_event) KeSetEvent(client_event, IO_NO_INCREMENT, FALSE);
        return finish(irp, STATUS_SUCCESS, 0);
    case 0x1d2013:
        if (client_event) KeClearEvent(client_event);
        return finish(irp, STATUS_SUCCESS, 0);
    case 0x1d201c:
        if (stack->Parameters.DeviceIoControl.OutputBufferLength != 0x410 ||
            !irp->AssociatedIrp.SystemBuffer)
            return finish(irp, STATUS_INVALID_PARAMETER, 0);
        if (*(ULONGLONG *)irp->AssociatedIrp.SystemBuffer == 1)
            return finish(irp, STATUS_SUCCESS, 0);
        RtlZeroMemory(irp->AssociatedIrp.SystemBuffer, 0x410);
        return finish(irp, STATUS_SUCCESS, 0x410);
    default:
        return finish(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    }
}

static void unload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING link;
    (void)driver;
    if (client_event) ObDereferenceObject(client_event);
    client_event = NULL;
    RtlInitUnicodeString(&link, L"\\DosDevices\\voicemodvad");
    IoDeleteSymbolicLink(&link);
    RtlInitUnicodeString(&link, L"\\DosDevices\\VMDriver");
    IoDeleteSymbolicLink(&link);
    if (control_device) IoDeleteDevice(control_device);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNICODE_STRING name, link;
    NTSTATUS status;
    ULONG i;
    (void)registry;
    RtlInitUnicodeString(&name, L"\\Device\\voicemodvad");
    RtlInitUnicodeString(&link, L"\\DosDevices\\voicemodvad");
    status = IoCreateDevice(driver, 0, &name, FILE_DEVICE_UNKNOWN, 0, FALSE,
                            &control_device);
    if (!NT_SUCCESS(status)) return status;
    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(control_device);
        control_device = NULL;
        return status;
    }
    RtlInitUnicodeString(&link, L"\\DosDevices\\VMDriver");
    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) {
        RtlInitUnicodeString(&link, L"\\DosDevices\\voicemodvad");
        IoDeleteSymbolicLink(&link);
        IoDeleteDevice(control_device);
        control_device = NULL;
        return status;
    }
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
        driver->MajorFunction[i] = dispatch;
    driver->DriverUnload = unload;
    control_device->Flags &= ~DO_DEVICE_INITIALIZING;
    DbgPrint("vmcompat: control device created\n");
    return STATUS_SUCCESS;
}
