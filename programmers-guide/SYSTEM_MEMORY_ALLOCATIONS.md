<!---

Copyright (C) 2022 Intel Corporation

SPDX-License-Identifier: MIT

-->

# System Memory Allocations in Level Zero

* [Introduction](#Introduction)
* [Limitations](#Limitations)
* [Debug Keys](#Debug-Keys)

# Introduction

System allocations, e.g. those created using `new` and `malloc`, are currently only supported on copy operations, such as `zeCommandListAppendMemoryCopy`. This means that if a GPU kernel requires access to a system allocation, it needs first to copy it to a USM allocation (whether host, device, or shared).

Whenever a system allocation is passed in a copy operation, the UMD (User Mode Driver, e.g. Level Zero) creates a graphics allocation that uses the system allocation as backing storage. This is needed in order to be able to pass to the KMD (Kernel Mode Driver) a command buffer containing the system allocation. This internal graphics allocation is then kept in the command list associated with the copy operation until the list is either reset or destroyed.

To create this internal graphics allocation, a call to the KMD is needed, which carries an expected overhead. Besides that, this graphics allocation is a new resource that needs to be made resident on the next submission, adding latency to it.

To avoid this overhead, it is recommended to do the following when using system allocations in an application:

* Perform warm-up iterations before the main critical loop of the application, so that the resource creation overhead is not counted towards performance of critical section.
* Avoid resetting the list to which a copy operation using the system allocation has been appended: New resource creation overhead is paid only on the first copy operation appended to the list. Subsequent copy operations will reuse the internal graphics allocation, until the list is reset or destroyed.
* Register the system memory allocation using the Import Extension API: To avoid having to create the internal graphics allocation every time a new list is used, or whenever a one is reset, the application can instruct the UMD to create the internal graphics allocation against a driver handle, instead of a command list handle. By doing this, the UMD is able to reuse the internal graphics allocation for any new or reset list, until the application decides to release the imported pointer. See below for the Import Extension APIs. An sample is provided in [zello_host_pointer.cpp](../../level_zero/core/test/black_box_tests/zello_host_pointer.cpp).

```cpp
zexDriverImportExternalPointer(
    ze_driver_handle_t hDriver,
    void *ptr,
    size_t size);

zexDriverReleaseImportedPointer(
    ze_driver_handle_t hDriver,
    void *ptr);

zexDriverGetHostPointerBaseAddress(
    ze_driver_handle_t hDriver,
    void *ptr,
    void **baseAddress);
```

# Limitations

Stack and static-allocated arrays have not been validated thoroughly.

# Debug Keys

* Applications can estimate the potential performance obtained from using the Import Extension APIs by setting `ForceHostPointerImport=1`. When the internal graphics allocation is being created as part of a copy operation and this debug key is set, the allocation is created against the driver handle, instead of the command list, producing effectively the same effect of calling  `zexDriverImportExternalPointer`. The use of this debug key is only recommended for debug purposes and testing, as it may produce memory leaks and data corruption because the internal graphics allocation is only released during the application tear-down.
