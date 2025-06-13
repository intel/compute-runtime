<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Level Zero Sysman Initialization

* [Introduction](#Introduction)
* [Initialization](#Initialization)
* [Support and Limitations](#Support-and-Limitations)
* [Mapping core device handle to sysman device handle](#Mapping-core-device-handle-to-sysman-device-handle-with-zesInit-initialization)
* [Recommendation](#Recommendation)

# Introduction

The following document describes limitations of using different initialization modes of System Resource Management Library (Sysman) in Level Zero. Implementation independent information on Level-Zero Sysman initialization are described in the Level-Zero specification [Sysman Programming Guide Section](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html#sysman-programming-guide).

# Initialization

An application can initialize Level Zero Sysman in following modes:

* [zeInit](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/api.html#zeinit) with [ZES_ENABLE_SYSMAN](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html#environment-variables) environment variable (also referenced as "Legacy mode" for brevity in this document).
* [zesInit](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesinit)

Psuedo code for the above can be referenced from [spec](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html#sysman-programming-guide).

# Support and Limitations

Following table summarizes the effect of using the specified initialization calls in a single user process.

| Initialization  Mode                                                                      | Core <-> Sysman Device Handle Casting | Core and Sysman device handle mapping                                                                           | Spec version Support                                                                                                                 | Platform Support                                  |
|-------------------------------------------------------------------------------------------|---------------------------------------|-----------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------|
| Legacy mode (zeInit +  ZES_ENABLE_SYSMAN)*                                                                               | Supported                             | Core <-> Sysman Device Handle Casting                                                                           | Supports features only upto v1.5. | Supported up to XeHPC (PVC) and earlier platforms |
| zesInit only                                                                                   | Not supported                         | [Sysman device mapping](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#sysmandevicemapping-functions) | Up to date with L0 Spec revision.                                                                                                              | Supported for all platforms.|
| zesInit + (zeInit W/o ZES_ENABLE_SYSMAN) Or <br> (zeInit W/o ZES_ENABLE_SYSMAN) + zesInit | Not supported                         | [Sysman device mapping](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#sysmandevicemapping-functions) | Up to date with L0 Spec revision.                                                                                                              | Supported for all platforms.|
| zesInit + (Legacy mode) Or <br> (Legacy mode) + zesInit                                   | Not supported                         | Not supported                                                                                                   | Not supported                                                                                                                        | Not supported                                     |

\* Initialization with Legacy mode is supported only if Level Zero Core is operating on [composite device hierarchy](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/core/PROG.html#device-hierarchy) model.<br>
\* Initialization with Legacy mode of sysman is supported on i915 KMD driver only.

# Mapping core device handle to sysman device handle with zesInit initialization 

When using zesInit based initialization, recommended way to retrieve sysman device handles is using zesDeviceGet() API. Typecasting L0 Core device handle to L0 Sysman device handle is not supported with zesInit based initialization of L0 Sysman. However, when working with both zeInit (for L0 Core) and zesInit (for L0 Sysman), if users would like to use L0 core device handle to retrieve L0 sysman device handle then L0 specification version 1.9 or above supports a experimental API [zesDriverGetDeviceByUuidExp()](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivergetdevicebyuuidexp) which can retrieve the L0 sysman device handle based on the valid UUID of the L0 core device handle. Prior to L0 specification version 1.9, L0 core device handle can be mapped to L0 sysman device handle by matching UUID of the L0 core device handle against the UUID of the available L0 sysman device handles(retrieved using zesDeviceGet()) in a loop till a match is found.

Following are the detailed steps to map a L0 core device handle to equivalent L0 sysman device handle when using L0 specification version 1.9 or above. The mapped L0 Sysman device handle can be used to work with L0 Sysman APIs. 

* Find the UUID of the core device handle using zeDeviceGetProperties()
* Get the Sysman driver handle(s) using zesDriverGet()
* Query the equivalent sysman device handle for core device handle by passing UUID and sysman driver handle to zesDriverGetDeviceByUuidExp(). 
* Use queried sysman device handle to access sysman functions in zesInit based initialization. 

Following is the sample source code for finding equivalent L0 sysman device handle for a given L0 core device handle. Sample code assumes that application has already initialized L0 sysman with zesInit() successfully.

```cpp
zes_device_handle_t getSysmanDeviceHandleFromCoreDeviceHandle(ze_device_handle_t hDevice)
{
    ze_device_properties_t deviceProperties = { ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES };
    ze_result_t result = zeDeviceGetProperties(hDevice, &deviceProperties);
    if (result != ZE_RESULT_SUCCESS) {
        printf("Error: zeDeviceGetProperties failed, result = %d\n", result); 
        return nullptr;
    }

    zes_uuid_t uuid = {};
    memcpy(uuid.id, deviceProperties.uuid.id, ZE_MAX_DEVICE_UUID_SIZE);

    uint32_t driverCount = 0;
    result = zesDriverGet(&driverCount, nullptr);
    if (driverCount == 0) {
        printf("Error could not retrieve driver\n");
        exit(-1);
    }
    zes_driver_handle_t* allDrivers = (zes_driver_handle_t*)malloc(driverCount * sizeof(zes_driver_handle_t));
    result = zesDriverGet(&driverCount, allDrivers);    
    if (result != ZE_RESULT_SUCCESS) {
        free(allDrivers);
        printf("Error:  zesDriverGet failed, result = %d\n", result);
        return nullptr;
    }

    zes_device_handle_t phSysmanDevice = nullptr;
    ze_bool_t onSubdevice = false;
    uint32_t subdeviceId = 0;
    for (int it = 0; it < driverCount; it++) {
        result = zesDriverGetDeviceByUuidExp(allDrivers[it], uuid, &phSysmanDevice, &onSubdevice, &subdeviceId);
        if (result == ZE_RESULT_SUCCESS && (phSysmanDevice != nullptr)) {
            break;
        }
    }   
    free(allDrivers);

    return phSysmanDevice;
}
```
Note: Along with mapped sysman device handle, `onSubdevice` and `subdeviceId` output parameters from zesDriverGetDeviceByUuidExp() are to be used for analysis of the L0 Sysman telemetry data on supported platforms. 

# Recommendation

1. Only one initialization of sysman is supported for a process and it is suggested that all libraries using sysman in the same process should use either legacy mode(zeInit + ZES_ENABLE_SYSMAN=1) or zesInit based initialization.
2. We recommend to use legacy mode(zeInit + ZES_ENABLE_SYSMAN=1) of sysman till PVC platforms.
3. For post PVC platforms, zesInit based sysman initialization should be used.
