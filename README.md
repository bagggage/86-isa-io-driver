# IO Windows Kernel Driver for x86/x86-64 CPUs

## I/O Ports

The driver provides a wrapper for `in`/`out` cpu instructions.

- *PortsIoRead*: `in` instruction wrapper
- *PortsWrite*: `out` instruction wrapper

## Memory-mapped I/O

For memory mapped i/o the driver provides mapping and read/write operations.

- **<span style="color:orange">WARNING:</span>**

  *Be careful with the write operation for MMIO, you can access any address in memory
  and therefore you can accidentally **damage** the memory.*

  *Also, don't forget to **MANDATORY** free up memory after using it via `Unmap`*

# Usage

Before, you have to compile or install driver. See [installation guide](/install/INSTALL.md).

Driver registers device and link it to a file named `ISA-IO`.
You might open it and then can use `IOCTL_*` commands to get access to the I/O ports and MMIO. 

Step-by-step:
- Include `Windows.h`.
- Copy and include header file `IO.h` into your userspace program.
- Open device handle like this:
  
  ```C
  HANDLE hDevice = CreateFileW(
	  L"\\??\\ISA-IO",
	  GENERIC_READ | GENERIC_WRITE,
	  FILE_SHARE_READ,
	  NULL,
	  OPEN_EXISTING,
	  FILE_ATTRIBUTE_NORMAL,
	  NULL
  );
  ```
- Now you can use driver [manually](#manual-use) or via [API](#api).

## Manual use

- Ports I/O write (`IOCTL_PIO_WRITE`):

  ```C
  PortsIoRequestWrite request;
  request.port = ...;
  request.value = ...;
  request.size = ...; // Can be `IO_BYTE`, `IO_WORD`, `IO_DWORD`.

  DeviceIoControl(hDevice, IOCTL_PIO_WRITE, &request, sizeof(request), NULL, 0, NULL, NULL);
  ```
- Ports I/O read (`IOCTL_PIO_READ`):
  ```C
  PortsIoRequestRead request;
  request.port = ...;
  request.size = ...; // Can be `IO_BYTE`, `IO_WORD`, `IO_DWORD`.

  PortsIoResponse response = { 0 };
  DeviceIoControl(hDevice, IOCTL_PIO_READ, &request, sizeof(request), &response, sizeof(response), NULL, NULL);

  // Access result
  ... = response.value;
  ```
- MMIO memory map (`IOCTL_MMAP_MMIO`):
  ```C++
  IoRequestMmap request;
  IoResponseMmap response;
  request.phys = physical;
  request.size = size;

  DeviceIoControl(
  	hDevice, IOCTL_MMAP_MMIO,
  	&request, sizeof(request),
  	&response, sizeof(response),
  	NULL, NULL
  )
  // Acces result
  ... = response.virt;
  ```
- MMIO memory unmap (`IOCTL_UMAP_MMIO`):
  ```C++
  IoRequestUnmap request;
  request.virt = virt;
  request.size = size;

  DeviceIoControl(hDevice, IOCTL_UMAP_MMIO, &request, sizeof(request), NULL, 0, NULL, NULL);
  ```
- MMIO read (`IOCTL_MMIO_READ_32`):
  ```C++
  IoRequestRead request;
  IoResponse response;
  request.virt = address;

  DeviceIoControl(
  	hDevice, IOCTL_MMIO_READ_32,
  	&request, sizeof(request),
  	&response, sizeof(response),
  	NULL, NULL
  )
  // Access result
  ... = response.value;
  ```
- MMIO write (`IOCTL_MMIO_WRITE_32`) (**unsafe**):
  ```C++
  IoRequestWrite request;
  request.virt = address;
  request.value = value;

  DeviceIoControl(hDevice, IOCTL_MMIO_WRITE_32, &request, sizeof(request), NULL, 0, NULL, NULL);
  ```

## API

These functions are provided in `IO.h`, their prototypes are provided here for a quick look.

- ```C++
  unsigned int PortsIoRead(HANDLE hDevice, unsigned short port, IoSize size)
  ```
- ```C++
  bool PortsIoWrite(HANDLE hDevice, unsigned short port, unsigned int value, IoSize size)
  ```
- ```C++
  void* MmIoMmap(HANDLE hDevice, void* physical, unsigned int size)
  ```
- ```C++
  bool MmIoUnmap(HANDLE hDevice, void* virt, unsigned int size)
  ```
- ```C++
  unsigned int MmIoRead(HANDLE hDevice, void* address)
  ```
- ```C++
  bool MmIoWrite(HANDLE hDevice, void* address, unsigned int value)
  ```
