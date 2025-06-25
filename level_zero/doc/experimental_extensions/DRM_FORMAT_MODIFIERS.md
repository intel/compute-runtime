<!---

Copyright (C) 2025 Intel Corporation

SPDX-License-Identifier: MIT

-->
# Level Zero Linux DRM Format Modifier Extension Specification

This document describes the proposed Level Zero Linux DRM Format Modifier Specification. The specification is intended to provide a way for applications to query and use Linux DRM format modifiers with Level Zero. The specification is intended to be used in conjunction with the Level Zero Interoperability Specification.

## Authors
- Aravind Gopalakrishnan (aravind.gopalakrishnan@intel.com)

## Contents

- [Summary](#summary)
    - [Motivation](#motivation)
    - [References](#references)
    - [Dependencies](#dependencies)
- [Description](#description)
    - [Overview of DRM Format Modifiers](#overview-of-drm-format-modifiers)
    - [LevelZero Image format translation](#levelzero-image-format-translation)
    - [Details](#details)
- [APIs](#new-entry-points)
    - [zeImageGetFormatModifierPropertiesExt](#zeimagegetformatmodifierpropertiesext)
    - [zeMemGetFormatModifierPropertiesExt](#zememgetformatmodifierpropertiesext)
- [Structures](#structures)
    - [ze_image_selected_format_modifier_ext_properties_t](#ze_image_selected_format_modifier_ext_properties_t)
    - [ze_image_format_modifier_create_list_ext_desc_t](#ze_image_format_modifier_create_list_ext_desc_t)
    - [ze_image_format_modifier_import_ext_desc_t](#ze_image_format_modifier_import_ext_desc_t)
    - [ze_mem_format_modifier_create_list_ext_desc_t](#ze_mem_format_modifier_create_list_ext_desc_t)
    - [ze_mem_selected_format_modifier_ext_properties_t](#ze_mem_selected_format_modifier_ext_properties_t)
    - [ze_mem_format_modifier_import_ext_desc_t](#ze_mem_format_modifier_import_ext_desc_t)
- [Example usage for DRM format modifiers with Images](#example-usage-for-drm-format-modifiers-with-images)
- [Example usage for DRM format modifiers with Buffers](#example-usage-for-drm-format-modifiers-with-buffers)

## Summary

### Motivation
This extension provides the ability to use DRM format modifiers with images, enabling LevelZero to better integrate with the Linux ecosystem of graphics, video, and display APIs.

Its functionality closely overlaps with VK_EXT_image_drm_format_modifier. This extension does not require the use of a specific handle type (such as a dma_buf) for external memory (similar to VK extension) and provides explicit control of image creation.

### References
- VK drm format modifier extension
- OCL drm format modifiers proposal

### Dependencies
This extension depends on the following extensions:
- ze_external_memory_import_* and ze_external_memory_export_* extensions

This extension does not depend or require the following extensions but works in conjunction with:
- "ZE_experimental_bindless_image"

## Description

### Overview of DRM Format Modifiers
(Content reused from VK spec for now)

A DRM format modifier is a 64-bit, vendor-prefixed, semi-opaque unsigned integer. Most modifiers represent a concrete, vendor-specific tiling format for images. Some exceptions are DRM_FORMAT_MOD_LINEAR (which is not vendor-specific); DRM_FORMAT_MOD_NONE (which is an alias of DRM_FORMAT_MOD_LINEAR due to historical accident); and DRM_FORMAT_MOD_INVALID (which does not represent a tiling format).

The modifier's vendor prefix consists of the 8 most significant bits. The canonical list of modifiers and vendor prefixes is found in drm_fourcc.h in the Linux kernel source. The other dominant source of modifiers are vendor kernel trees. Further documentation can also be found in Linux kernel tree and on VK specification.

### LevelZero Image format translation
Modifier-capable APIs often pair modifiers with DRM formats, which are defined in drm_fourcc.h. When using Levelzero, the application must convert between ze_image_format_layout_t and DRM format when it sends or receives a DRM format to or from an external API.

The mapping from the L0 image format layout to DRM format is lossy. Therefore, when receiving a DRM format from an external API, often the application must use information from the external API to accurately map the DRM format to a L0 format. For example, DRM formats do not distinguish between RGB and sRGB (as of 2018-03-28); external information is required to identify the image's color space.

The mapping between L0 format and DRM format is also incomplete. For some DRM formats there exist no corresponding L0 format, and for some L0 formats there may exist no corresponding DRM format.

### Details

#### DRM format modifiers for images
There are three main phases to using DRM format modifiers with Level Zero:
1. Querying the device for supported DRM format modifiers (Negotiation/Discovery phase)
2. Creating or importing an image with a DRM format modifier (Create/Import phase)
3. Using the image with an external API (Export phase)

Different interfaces are provided for each phase.

1. **Querying the device for supported DRM format modifiers (Negotiation/Discovery phase)**
   
    - Provide the image descriptor to LevelZero to gather supported DRM format modifiers for a given image format
    - Query the supported DRM format modifiers using:
        ```c
        zeIntelImageGetFormatModifiersSupportedExp
        ```
    - Gather a set of DRM format modifiers from the external API (such as Vulkan) for interoperability
    - Intersect the DRM format modifiers from the external API with those supported by LevelZero to find a common set
    - Provide this "common set" to the external API for creating an image with the exporter API
    - Export the image from the external API
    - Import the image into LevelZero by providing the single DRM format modifier value that the exporter used for resource creation

    The example usages are shown in sections [Example usage for DRM format modifiers with Images](#example-usage-for-drm-format-modifiers-with-images) and [Example usage for DRM format modifiers with Buffers](#example-usage-for-drm-format-modifiers-with-buffers).

2. **Creating an image with a DRM format modifier (Create/Import phase)**

   - Creating L0 image with a DRM format modifier: The application creates an L0 image with a DRM format modifier. The application can provide a list of DRM format modifiers by chaining them via pNext to ze_image_desc_t. The implementation chooses one from the list of DRM format modifiers as it sees fit.
   - Importing an image with DRM format modifier: If the application wants to import an image into L0, then it must pass a single DRM format modifier using ze_image_format_modifier_import_ext_desc_t and pass it via pNext chain to ze_image_desc_t.
   - When drm format modifiers are not used during image creation or when importing from another API, then implementation assumes that images, buffers and USM device/shared allocations are non compressed and linear

   ```c
   ze_intel_image_format_modifier_create_list_exp_desc_t
   ze_intel_image_format_modifier_import_exp_desc_t
   ```

   Above structs are used in conjunction with the ze_image_desc_t structure and the user sets pNext chain accordingly in ze_image_desc_t. For example to import, pNext of ze_image_desc_t points to ze_external_memory_import_desc_t and ze_external_memory_import_desc_t points to ze_image_format_modifier_import_ext_desc_t

3. **Using the image with an external API (Export phase)**
   
   The application uses the image with an external API. The application can query the chosen DRM format modifier for the image. This is useful if the application wants to export the image to another API that requires the DRM format modifier. To this end, a new extension struct is provided that can be chained to pNext of ze_image_properties_t:

   ```c
   ze_intel_image_selected_format_modifier_exp_properties_t
   ```

   Using above drmFormatModifier info, the application can pair it with ze_external_memory_export_desc_t that is chained to ze_image_desc_t to export the image to another API.

#### DRM format modifiers for buffers
The procedure for using DRM format modifiers with buffers is similar to that of images. 

- The application queries LevelZero by providing the buffer descriptor to gather supported DRM format modifiers for a given buffer format.
- The application can then use this information to choose a DRM format modifier for the buffer during creation.
- The set of drm format modifiers returned by implementation must be correct for the buffer descriptor provided.
- DRM format modifiers for buffers are particularly useful if we want to interop with buffers that could be compressed.

The application can use the following entry point to query the supported DRM format modifiers:

```c
zeIntelMemGetFormatModifiersSupportedExp
```

The application creates a buffer with a DRM format modifier. The application can provide a list of DRM format modifiers by chaining them via pNext to ze_device_mem_alloc_desc_t. The implementation chooses one from the list of DRM format modifiers as it sees fit. The application can also import a buffer with a DRM format modifier. If the application wants to import a buffer into L0, then it must pass a single DRM format modifier using ze_intel_mem_format_modifier_import_exp_desc_t and pass it via pNext chain to ze_device_mem_alloc_desc_t.

```c
ze_intel_mem_format_modifier_create_list_exp_desc_t
ze_intel_mem_format_modifier_import_exp_desc_t
```

Above structs are used in conjunction with the ze_device_mem_alloc_desc_t structure and the user sets pNext chain accordingly in ze_device_mem_alloc_desc_t. For example to import, pNext of ze_device_mem_alloc_desc_t points to ze_external_memory_import_desc_t and ze_external_memory_import_desc_t points to ze_intel_mem_format_modifier_import_exp_desc_t

**When drm format modifiers are not used during buffer creation or when importing from another API, then implementation assumes that buffers are non compressed.**

The application can query the chosen DRM format modifier for the buffer. This is useful if the application wants to export the buffer to another API that requires the DRM format modifier. To this end, a new extension struct is provided that can be chained to pNext of ze_memory_allocation_properties_t:

```c
ze_intel_mem_selected_format_modifier_exp_properties_t
```

Using above drmFormatModifier info, the application can pair it with ze_external_memory_export_desc_t that is chained to ze_device_mem_alloc_desc_t to export the buffer to another API.

## New Entry Points

### zeIntelImageGetFormatModifiersSupportedExp

`zeIntelImageGetFormatModifiersSupportedExp` is used to query for supported DRM format modifiers for a given image. The user would use the information to either:
1. Create L0 image with the list of DRM format modifiers provided by the query
2. Use the list provided here to compare with a set of DRM format modifiers gathered from another API (such as Vulkan) and then create an intersection of the two lists so they have a common set of DRM format modifiers that can be used for interop. The user would proceed to provide the "common" set to external API for creating image and export the image before importing into L0.

```c
zeIntelImageGetFormatModifiersSupportedExp(

    ze_device_handle_t hDevice,           ///< [in] handle of the device
    const ze_image_desc_t* pImageDesc,    ///< [in] pointer to image descriptor
    uint32_t* pCount,                     ///< [in,out] pointer to the number of DRM format modifiers.
                                          ///< if count is zero, then the driver shall update the value with the
                                          ///< total number of supported DRM format modifiers for the image format.
                                          ///< if count is greater than the number of supported DRM format modifiers,
                                          ///< then the driver shall update the value with the correct number of supported DRM format modifiers.
    uint64_t* pDrmFormatModifiers        ///< [in,out][optional][range(0, *pCount)] array of supported DRM format modifiers
);
```

returns ZE_RESULT_SUCCESS if the drm modifier is supported for the image descriptor, else returns ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT.

### zeIntelMemGetFormatModifiersSupportedExp

`zeIntelMemGetFormatModifiersSupportedExp` is used to query for supported DRM format modifiers for a given buffer. The user would use the information to create L0 memory allocation.

```c
zeIntelMemGetFormatModifiersSupportedExp(
    ze_context_handle_t hContext,                 ///< [in] handle of the context
    const ze_device_mem_alloc_desc_t* pDeviceDesc, ///< [in] pointer to device memory allocation descriptor
    size_t size,                                  ///< [in] size in bytes to allocate
    size_t alignment,                             ///< [in] minimum alignment in bytes for the allocation
    ze_device_handle_t hDevice,                   ///< [in] handle of the device
    uint32_t* pCount,                             ///< [in,out] pointer to the number of DRM format modifiers.
                                                  ///< if count is zero, then the driver shall update the value with the
                                                  ///< total number of supported DRM format modifiers for the memory allocation.
                                                  ///< if count is greater than the number of supported DRM format modifiers,
                                                  ///< then the driver shall update the value with the correct number of supported DRM format modifiers.
    uint64_t* pDrmFormatModifiers                 ///< [in,out][optional][range(0, *pCount)] array of supported DRM format modifiers
);
```

## Structures

### ze_intel_image_selected_format_modifier_exp_properties_t

Properties struct for providing user with the selected drm format modifier for the image. This is useful if the application wants to export the image to another API that requires the DRM format modifier. The application can query the chosen DRM format modifier for the image. The application can use this information to choose a DRM format modifier for the image during creation.

```c
typedef struct _ze_intel_image_selected_format_modifier_exp_properties_t {
    ze_structure_type_t         stype;          ///< derived from ze_base_properties_t
    void*                 pNext;                ///< derived from ze_base_properties_t
    uint64_t drmFormatModifier;                 ///< Linux drm format modifier 
} ze_intel_image_selected_format_modifier_exp_properties_t;
```

### ze_intel_image_format_modifier_create_list_exp_desc_t

Descriptor for creating image with the specified list of drm format modifiers. The pNext chain is setup accordingly in ze_image_desc_t prior to calling zeImageCreate API. If the user passes a list struct, then implementation chooses one from the list of drm modifiers as it sees fit. If user wants to pass a single drm modifier then they can set the drmFormatModifierCount to 1 and pass the single drm modifier in pDrmFormatModifiers.

```c
typedef struct _ze_intel_image_format_modifier_create_list_exp_desc_t {
    ze_structure_type_t stype;          ///< derived from ze_base_desc_t
    const void* pNext;                  ///< derived from ze_base_desc_t
    uint32_t drmFormatModifierCount;    ///< number of modifiers compatible with format
    uint64_t* pDrmFormatModifiers;      ///< pointer to array of Linux drm format modifiers
} ze_intel_image_format_modifier_create_list_exp_desc_t;
```

### ze_intel_image_format_modifier_import_exp_desc_t

### ze_intel_image_format_modifier_import_exp_desc_t

Descriptor for importing image with the specified drm format modifier. The pNext chain is setup accordingly in ze_image_desc_t prior to calling zeImageCreate API. For example, when using interop, pNext of ze_image_desc_t points to ze_external_memory_import_desc_t and ze_external_memory_import_desc_t points to ze_intel_image_format_modifier_import_exp_desc_t.

```c
typedef struct _ze_intel_image_format_modifier_import_exp_desc_t {
    ze_structure_type_t stype;          ///< derived from ze_base_desc_t
    const void* pNext;                  ///< derived from ze_base_desc_t
    uint64_t pDrmFormatModifiers;      ///< Linux drm format modifier value for import
} ze_intel_image_format_modifier_import_exp_desc_t;
```

### ze_intel_mem_format_modifier_create_list_exp_desc_t

Descriptor for creating buffer with the specified list of drm format modifiers. The pNext chain is setup accordingly in ze_device_mem_alloc_desc_t prior to calling zeMemAllocDevice API. If the user passes a list struct, then implementation chooses one from the list of drm modifiers as it sees fit.

```c
typedef struct _ze_intel_mem_format_modifier_create_list_exp_desc_t {
    ze_structure_type_t stype;          ///< derived from ze_base_desc_t
    const void* pNext;                  ///< derived from ze_base_desc_t
    uint32_t drmFormatModifierCount;    ///< number of modifiers compatible with format
    uint64_t* pDrmFormatModifiers;      ///< pointer to array of Linux drm format modifiers
} ze_intel_mem_format_modifier_create_list_exp_desc_t;
```

### ze_intel_mem_selected_format_modifier_exp_properties_t

Struct for providing user with the selected drm format modifier for the buffer. This is useful if the application wants to export the buffer to another API that requires the DRM format modifier. This struct can be chained to pNext of ze_memory_allocation_properties_t and invoked via zeMemGetAllocProperties API.

```c
typedef struct _ze_intel_mem_selected_format_modifier_exp_properties_t {
    ze_structure_type_t         stype;          ///< derived from ze_base_properties_t
    void*                 pNext;                ///< derived from ze_base_properties_t
    uint64_t drmFormatModifier;                 ///< Linux drm format modifier 
} ze_intel_mem_selected_format_modifier_exp_properties_t;
```

### ze_intel_mem_format_modifier_import_exp_desc_t

Descriptor for importing buffer with the specified drm format modifier. The pNext chain is setup accordingly in ze_device_mem_alloc_desc_t prior to calling zeMemAllocDevice API. For example, when using interop, pNext of ze_device_mem_alloc_desc_t points to ze_external_memory_import_desc_t and ze_external_memory_import_desc_t points to ze_intel_mem_format_modifier_import_exp_desc_t.

```c
typedef struct _ze_intel_mem_format_modifier_import_exp_desc_t {
    ze_structure_type_t stype;          ///< derived from ze_base_desc_t
    const void* pNext;                  ///< derived from ze_base_desc_t
    uint64_t pDrmFormatModifier;      ///< Linux drm format modifier value for import
} ze_intel_mem_format_modifier_import_exp_desc_t;
```

## Example usage for DRM format modifiers with Images

```cpp
#include <level_zero/ze_api.h>

// Discovery/Negotiation phase for DRM format modifiers
// Single-precision float image format with one channel
ze_image_format_t imageFormat = {
    .layout = ZE_IMAGE_FORMAT_LAYOUT_32,
    .type = ZE_IMAGE_FORMAT_TYPE_FLOAT,
    .x =  ZE_IMAGE_FORMAT_SWIZZLE_R,
    .y = ZE_IMAGE_FORMAT_SWIZZLE_X,
    .z = ZE_IMAGE_FORMAT_SWIZZLE_R,
    .w = ZE_IMAGE_FORMAT_SWIZZLE_X
}

ze_image_desc_t imageDesc = {
    .stype = ZE_STRUCTURE_TYPE_IMAGE_DESC,
    .pNext = nullptr,
    .flags = 0,
    .type = ZE_IMAGE_TYPE_2D,
    .format = imageFormat,
    .width = 128,
    .height = 128,
    .depth = 0,
    .arraylevels = 0,
    .miplevels = 0
};

// DRM format modifier negotiation

uint32_t drmFormatModifierCount = 0;

zeIntelImageGetFormatModifiersSupportedExp(
    device,
    &imageDesc,
    &drmFormatModifierCount,
    nullptr
);
if (drmFormatModifierCount == 0) {
    // No supported DRM format modifiers
    return ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT;
}
std::vector<uint64_t> drmFormatModifiers(drmFormatModifierCount);
zeIntelImageGetFormatModifiersSupportedExp(
    device,
    &imageDesc,
    &drmFormatModifierCount,
    drmFormatModifiers.data()
);

// Scenario 1. Creating L0 image with a DRM format modifier for exporting purposes
ze_intel_image_format_modifier_create_list_exp_desc_t createListDesc = {
    .stype = ZE_STRUCTURE_TYPE_IMAGE_FORMAT_MODIFIER_CREATE_LIST_EXT_DESC,
    .pNext = nullptr,
    .drmFormatModifierCount = drmFormatModifierCount,
    .pDrmFormatModifiers = drmFormatModifiers.data()
};

ze_external_memory_export_desc_t export_desc = {
    .stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC,
    .pNext = &createListDesc, // pNext
    .flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD,
};
imageDesc.pNext = &export_desc;

zeImageCreate(
    context,
    device,
    &imageDesc,
    &hImage
);

// Scenario 2. Importing an image with a DRM format modifier

// For interoperability:
// 1. Get DRM format modifiers from both L0 and the external API (e.g., Vulkan)
// 2. Find the intersection to determine compatible modifiers
// 3. Create the external resource using the common set of modifiers
// 4. Query which specific modifier was chosen by the implementation
// 5. Use that modifier when importing into L0

// Example with Vulkan:
// - Use vkGetPhysicalDeviceFormatProperties2 with VkDrmFormatModifierPropertiesListEXT
// - Verify compatibility with vkGetPhysicalDeviceImageFormatProperties2
// - Create VK image with VkImageDrmFormatModifierListCreateInfoEXT
// - Query the chosen modifier with vkGetImageDrmFormatModifierPropertiesEXT
// - Import into L0 as shown below

ze_intel_image_format_modifier_import_exp_desc_t importDrmModifierDesc = {
    .stype = ZE_STRUCTURE_TYPE_IMAGE_FORMAT_MODIFIER_IMPORT_EXT_DESC,
    .pNext = nullptr,
    .pDrmFormatModifiers = drmFormatImportModifier // drm format modifier used to create image with exporter
};

// Set up the request to import the external memory handle
ze_external_memory_import_fd_t import_fd = {
    .stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD,
    .pNext = &importDrmModifierDesc,
    .flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD,
    .fd = fd
};

imageDesc.pNext = &import_fd;

// User can also use bindless images extension here and can chain the pNext to above struct (not shown in this example)
ze_result_t result = zeImageCreate(
    context,
    device,
    &imageDesc,
    &hImage
);
if (result != ZE_RESULT_SUCCESS) {
    // Handle error
}

// Exporting the image with a DRM format modifier (when user has created an L0 image)
// The application can query the chosen DRM format modifier for the image
ze_intel_image_selected_format_modifier_exp_properties_t selectedFormatModifierDesc = {
    .stype = ZE_STRUCTURE_TYPE_IMAGE_SELECTED_FORMAT_MODIFIER_EXT_PROPERTIES,
    .pNext = nullptr,
    .drmFormatModifier = 0
};
ze_image_properties_t imageProperties = {};
imageProperties.stype = ZE_STRUCTURE_TYPE_IMAGE_PROPERTIES;
imageProperties.pNext = &selectedFormatModifierDesc;
zeImageGetProperties(
    device,
    &imageDesc,
    &imageProperties
);
```

## Example usage for DRM format modifiers with Buffers

```cpp
#include <level_zero/ze_api.h>
// Discovery/Negotiation phase for DRM format modifiers

uint32_t drmFormatModifierCount = 0;
ze_device_mem_alloc_desc_t deviceDesc = {
    .stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
    .pNext = nullptr,
    .flags = 0,
    .ordinal = 0
};

size_t size = 4096;
size_t alignment = 0;
zeIntelMemGetFormatModifiersSupportedExp(
    context,
    &deviceDesc,
    size,
    alignment,
    device,
    &drmFormatModifierCount,
    nullptr
);

if (drmFormatModifierCount == 0) {
    // No supported DRM format modifiers
    return ZE_RESULT_ERROR_UNKNOWN;
}
std::vector<uint64_t> drmFormatModifiers(drmFormatModifierCount);
zeIntelMemGetFormatModifiersSupportedExp(
    context,
    &deviceDesc,
    size,
    alignment,
    device,
    &drmFormatModifierCount,
    drmFormatModifiers.data()
);

// Scenario 1. Creating L0 buffer with a DRM format modifier for exporting purposes
ze_intel_mem_format_modifier_create_list_exp_desc_t createListDesc = {
    .stype = ZE_STRUCTURE_TYPE_MEM_FORMAT_MODIFIER_CREATE_LIST_EXT_DESC,
    .pNext = nullptr,
    .drmFormatModifierCount = drmFormatModifierCount,
    .pDrmFormatModifiers = drmFormatModifiers.data()
};

ze_external_memory_export_desc_t export_desc = {
    .stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC,
    .pNext = &createListDesc,
    .flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD,
};
deviceDesc.pNext = &export_desc;

void *ptr = nullptr;
ze_result_t result = zeMemAllocDevice(
    context,
    &deviceDesc,
    size,
    alignment,
    device,
    &ptr
);

if (result != ZE_RESULT_SUCCESS) {
    // Handle error
}

// Exporting the buffer with a DRM format modifier (when user has created an L0 buffer)
// The application can query the chosen DRM format modifier for the buffer
ze_intel_mem_selected_format_modifier_exp_properties_t selectedFormatModifierDesc = {
    .stype = ZE_STRUCTURE_TYPE_MEM_SELECTED_FORMAT_MODIFIER_EXT_PROPERTIES,
    .pNext = nullptr,
    .drmFormatModifier = 0
};
ze_external_memory_export_fd_t export_fd = {
    .stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD,
    .pNext = selectedFormatModifierDesc,
    .flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD,
    .fd = 0 
};

// Link export request into query
ze_memory_allocation_properties_t memProperties = {};
memProperties.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
memProperties.pNext = &export_fd;

zeMemGetAllocProperties(
    context,
    ptr,
    &memProperties,
    device
);

// Scenario 2. Importing a buffer with a DRM format modifier
// For interoperability:
// 1. Intersect DRM format modifiers from external API with those supported by L0
// 2. Create resource in external API using compatible modifiers
// 3. Extract the specific modifier used by the external API
// 4. Provide this modifier during import
// Note: The specific protocol for extracting modifiers from exporters is out of scope for this example

ze_intel_mem_format_modifier_import_exp_desc_t importDrmModifierDesc = {
    .stype = ZE_STRUCTURE_TYPE_MEM_FORMAT_MODIFIER_IMPORT_EXT_DESC,
    .pNext = nullptr,
    .drmFormatModifier = drmFormatImportModifier // drm format modifier used to create image with exporter
};
// Set up the request to import the external memory handle
ze_external_memory_import_fd_t import_fd = {
    .stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD,
    .pNext = &importDrmModifierDesc,
    .flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD,
    .fd = fd
};

deviceDesc.pNext = &import_fd;

ze_result_t result = zeMemAllocDevice(
    context,
    &deviceDesc,
    size,
    alignment,
    device,
    &ptr
);
if (result != ZE_RESULT_SUCCESS) {
    // Handle error
}
```
