<!---

Copyright (C) 2024 Intel Corporation

SPDX-License-Identifier: MIT

-->

# 2D Block Array Transpose Properties Extension

* [Overview](#Overview)
* [Definitions](#Definitions)
* [Known Issues and Limitations](#Known-Issues-and-Limitations)

# Overview

Some key framework optimization replies on 2D-block-load transpose feature of GPU product, but this feature is absent in some Intel GPU products. 
This extension allows for users to query the 2D-block-load transpose capabilities of a device.

# Definitions

## Interfaces

```cpp
///////////////////////////////////////////////////////////////////////////////
/// @brief Supported 2D Block Array flags
typedef uint32_t ze_intel_device_block_array_exp_flags_t;
typedef enum _ze_intel_device_block_array_exp_flag_t {
    ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE = ZE_BIT(0), ///< Supports store operation
    ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD = ZE_BIT(1),  ///< Supports load operation
    ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_FORCE_UINT32 = 0x7fffffff

} ze_intel_device_block_array_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
#ifndef ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_NAME
/// @brief Device 2D block array properties driver extension name
#define ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_NAME "ZE_intel_experimental_device_block_array_properties"
#endif // ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_NAME

/// @brief Device 2D block array properties queried using
///        ::zeDeviceGetProperties
///
/// @details
///     - This structure may be passed to ::zeDeviceGetProperties, via
///       `pNext` member of ::ze_device_properties_t.
/// @brief Device 2D block array properties
#define ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES (ze_structure_type_ext_t)0x00030007

typedef struct _ze_intel_device_block_array_exp_properties_t {
    ze_structure_type_ext_t stype = ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES; ///< [in] type of this structure
    void *pNext;                                                                ///< [in,out][optional] must be null or a pointer to an extension-specific
                                                                            ///< structure (i.e. contains sType and pNext).
    ze_intel_device_block_array_exp_flags_t flags;                          ///< [out] 0 (none) or a valid combination of ::ze_intel_device_block_array_exp_flag_t
} ze_intel_device_block_array_exp_properties_t;
```

## Programming example

```cpp

    ze_device_roperties_t deviceProps = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    ze_intel_device_block_array_exp_properties_t blockArrayProps = {ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES};
    deviceProps.pNext = &blockArrayProps;

    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProps));

    if (blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE) {
        printf("2D block store supported\n");
    }

    if (blockArrayProps.flags & ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD) {
        printf("2D block load supported\n");
    }

```

# Known Issues and Limitations

