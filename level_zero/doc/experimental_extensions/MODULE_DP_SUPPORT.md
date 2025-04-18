<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Device Module Dot Product Properties Extension

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

Users often need information regarding Dot Product (DP) support available in platform prior to adding reliable GPU support. This extension provides a new property to gather the required platform support information from driver.

# Definitions

## Interfaces

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Supported Dot Product flags
typedef uint32_t ze_intel_device_module_dp_exp_flags_t;
typedef enum _ze_intel_device_module_dp_exp_flag_t {
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A = ZE_BIT(0), ///< Supports DP4A operation
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS = ZE_BIT(1), ///< Supports DPAS operation
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_intel_device_module_dp_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
#define ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES (ze_structure_type_ext_t)0x00030013
///////////////////////////////////////////////////////////////////////////////
/// @brief Device Module dot product properties queried using
///        ::zeDeviceGetModuleProperties
///
/// @details
///     - This structure may be passed to ::zeDeviceGetModuleProperties, via
///       `pNext` member of ::ze_device_module_properties_t.
/// @brief Device module dot product properties
typedef struct _ze_intel_device_module_dp_exp_properties_t {
    ze_structure_type_ext_t stype = ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES; ///< [in] type of this structure
    void *pNext;                                                                        ///< [in,out][optional] must be null or a pointer to an extension-specific
                                                                                        ///< structure (i.e. contains sType and pNext).
    ze_intel_device_module_dp_exp_flags_t flags;                                        ///< [out] 0 (none) or a valid combination of ::ze_intel_device_module_dp_flag_t
} ze_intel_device_module_dp_exp_properties_t;
```

## Programming example

```cpp

    ze_device_module_properties_t deviceModProps = {ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES};
    ze_intel_device_module_dp_exp_properties_t moduleDpProps = {ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES};
    deviceModProps.pNext = &moduleDpProps;

    SUCCESS_OR_TERMINATE(zeDeviceGetModuleProperties(device, &deviceModProps));

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A) {
        printf("DP4A supported\n");
    }

    if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS) {
        printf("DPAS supported\n");
    }

```

# Known Issues and Limitations

