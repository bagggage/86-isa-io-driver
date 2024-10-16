#include <ntddk.h>
#include <wdf.h>

#define KERNEL_SPACE

#include "IO.h"

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD IsaIoEvtDeviceAdd;

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    WDF_DRIVER_CONFIG config;

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "ISA IO: Initialization...\n"));

    // Initialize the driver configuration object to register the
    // entry point for the EvtDeviceAdd callback, IsaIoEvtDeviceAdd
    WDF_DRIVER_CONFIG_INIT(&config,
        IsaIoEvtDeviceAdd
    );

    // Finally, create the driver object
    NTSTATUS status = WdfDriverCreate(DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    return status;
}

VOID* GetInputBuffer(WDFREQUEST request, ULONG size) {
    PVOID buffer = NULL;
    size_t length = 0;

    WdfRequestRetrieveInputBuffer(request, size, &buffer, &length);

    if (!buffer || length < size) return NULL;

    return buffer;
}

VOID* GetOutputBuffer(WDFREQUEST request, ULONG size) {
    PVOID buffer = NULL;
    size_t length = 0;

    WdfRequestRetrieveOutputBuffer(request, size, &buffer, &length);

    if (!buffer || length < size) return NULL;

    return buffer;
}

VOID HandleIOCTL(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    size_t returnBytes = 0;

    switch (IoControlCode)
    {
    case IOCTL_PIO_READ: {
        struct PortsIoRequestRead request_data = { 0 };
        struct PortsIoResponse* response_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(struct PortsIoRequestRead));
        PVOID outputBuffer = GetOutputBuffer(Request, sizeof(struct PortsIoResponse));

        if (!buffer || !outputBuffer) {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        request_data = *((struct PortsIoRequestRead*)buffer);
        response_data = (struct PortsIoResponse*)outputBuffer;

        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "PIO: Read: 0x%x\n", request_data.port));

        // Read ISA IO and put result into a buffer
        switch (request_data.size)
        {
        case IO_BYTE:
            response_data->value = __inbyte(request_data.port);
            break;
        case IO_WORD:
            response_data->value = __inword(request_data.port);
            break;
        case IO_DWORD:
            response_data->value = __indword(request_data.port);
            break;
        default:
            goto bad_input;
        }

        returnBytes = sizeof(struct PortsIoResponse);

        break;
    }
    case IOCTL_PIO_WRITE: {
        struct PortsIoRequestWrite request_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(struct PortsIoRequestWrite));
        if (!buffer) {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        request_data = *((struct PortsIoRequestWrite*)buffer);

        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "PIO: Write: 0x%x = 0x%x\n", request_data.port, request_data.value));

        // Write PIO
        switch (request_data.size)
        {
        case IO_BYTE:
            __outbyte(request_data.port, (UCHAR)request_data.value);
            break;
        case IO_WORD:
            __outword(request_data.port, (USHORT)request_data.value);
            break;
        case IO_DWORD:
            __outdword(request_data.port, request_data.value);
            break;
        case IO_QWORD:
            break;
        default:
            goto bad_input;
            break;
        }

        break;
    }
    case IOCTL_MMAP_MMIO: {
        struct IoRequestMmap request_data = { 0 };
        struct IoResponseMmap* response_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(request_data));
        PVOID outputBuffer = GetOutputBuffer(Request, sizeof(*response_data));

        if (!buffer || !outputBuffer) {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        request_data = *((struct IoRequestMmap*)buffer);
        response_data = ((struct IoResponseMmap*)outputBuffer);

        PHYSICAL_ADDRESS phys;
        phys.QuadPart = (LONGLONG)request_data.phys;

        response_data->virt = MmMapIoSpace(phys, request_data.size, MmNonCached);
        returnBytes = sizeof(struct IoResponseMmap);

        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MMIO: Mapped: 0x%x -> 0x%x\n", request_data.phys, response_data->virt));

        break;
    }
    case IOCTL_UMAP_MMIO: {
        struct IoRequestUnmap request_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(request_data));

        if (!buffer) {
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }

        request_data = *((struct IoRequestUnmap*)buffer);
        
        MmUnmapIoSpace(request_data.virt, request_data.size);

        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "MMIO: Unmapped: 0x%x\n", request_data.virt));

        break;
    }
    case IOCTL_MMIO_READ_32: {
        struct IoRequestRead request_data = { 0 };
        IoResponse* response_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(request_data));
        PVOID outputBuffer = GetOutputBuffer(Request, sizeof(*response_data));

        request_data = *((struct IoRequestRead*)buffer);
        response_data = (IoResponse*)outputBuffer;

        response_data->value = *((unsigned int*)request_data.virt);
        returnBytes = sizeof(IoResponse);

        break;
    }
    case IOCTL_MMIO_WRITE_32: {
        struct IoRequestWrite request_data = { 0 };

        PVOID buffer = GetInputBuffer(Request, sizeof(request_data));
        request_data = *((struct IoRequestWrite*)buffer);

        *((unsigned int*)request_data.virt) = request_data.value;

        break;
    }
    default:
    {
    bad_input:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }
    }

    if (returnBytes > 0)
        WdfRequestCompleteWithInformation(Request, status, returnBytes);
    else
        WdfRequestComplete(Request, status);
}

DECLARE_CONST_UNICODE_STRING(
    SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
    L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
);

NTSTATUS
IsaIoEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    // We're not using the driver object,
    // so we need to mark it as unreferenced
    UNREFERENCED_PARAMETER(Driver);

    NTSTATUS status;

    UNICODE_STRING symLinkName = { 0 };
    UNICODE_STRING deviceFileName = { 0 };

    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\ISA-IO");
    RtlInitUnicodeString(&deviceFileName, L"\\Device\\ISA_IO_Dev");

    WdfDeviceInitAssignSDDLString(DeviceInit, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    WdfDeviceInitSetCharacteristics(DeviceInit, FILE_DEVICE_SECURE_OPEN, FALSE);

    status = WdfDeviceInitAssignName(
        DeviceInit,
        &deviceFileName
    );

    if (!NT_SUCCESS(status)) {
        WdfDeviceInitFree(DeviceInit);
        return status;
    }

    WDFDEVICE hDevice;

    // Create the device object
    status = WdfDeviceCreate(
        &DeviceInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hDevice
    );

    if (!NT_SUCCESS(status)) {
        WdfDeviceInitFree(DeviceInit);
        return status;
    }

    status = WdfDeviceCreateSymbolicLink(
        hDevice,
        &symLinkName
    );

    WDF_IO_QUEUE_CONFIG  ioQueueConfig;
    WDFQUEUE  hQueue;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
        &ioQueueConfig,
        WdfIoQueueDispatchSequential
    );


    ioQueueConfig.EvtIoDeviceControl = HandleIOCTL;

    status = WdfIoQueueCreate(
        hDevice,
        &ioQueueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &hQueue
    );

    if (!NT_SUCCESS(status)) {
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "ISA IO: Successfull\n"));

    return status;
}

