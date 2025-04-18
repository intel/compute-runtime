/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/surface_format_info.h"

#include "level_zero/driver_experimental/ze_bindless_image_exp.h"
#include "level_zero/driver_experimental/zex_common.h"
#include <level_zero/ze_api.h>

#include <cstdint>
#include <optional>

namespace L0 {
inline NEO::ImageType convertType(const ze_image_type_t type) {
    switch (type) {
    case ZE_IMAGE_TYPE_2D:
        return NEO::ImageType::image2D;
    case ZE_IMAGE_TYPE_3D:
        return NEO::ImageType::image3D;
    case ZE_IMAGE_TYPE_2DARRAY:
        return NEO::ImageType::image2DArray;
    case ZE_IMAGE_TYPE_1D:
        return NEO::ImageType::image1D;
    case ZE_IMAGE_TYPE_1DARRAY:
        return NEO::ImageType::image1DArray;
    case ZE_IMAGE_TYPE_BUFFER:
        return NEO::ImageType::image1DBuffer;
    default:
        break;
    }
    return NEO::ImageType::invalid;
}

inline NEO::ImageDescriptor convertDescriptor(const ze_image_desc_t &imageDesc) {
    NEO::ImageDescriptor desc = {};
    desc.fromParent = false;
    desc.imageArraySize = imageDesc.arraylevels;
    desc.imageDepth = imageDesc.depth;
    desc.imageHeight = imageDesc.height;
    desc.imageRowPitch = 0u;
    desc.imageSlicePitch = 0u;
    desc.imageType = convertType(imageDesc.type);
    desc.imageWidth = imageDesc.width;
    desc.numMipLevels = imageDesc.miplevels;
    desc.numSamples = 0u;
    return desc;
}

struct StructuresLookupTable {
    struct ImageProperties {
        NEO::ImageDescriptor imageDescriptor;
        uint32_t planeIndex;
        bool isPlanarExtension;
        void *pitchedPtr;
        const ze_sampler_desc_t *samplerDesc;
    } imageProperties;

    struct SharedHandleType {
        void *ntHandle;
        int fd;
        bool isSupportedHandle;
        bool isOpaqueFDHandle;
        bool isDMABUFHandle;
        bool isNTHandle;
    } sharedHandleType;

    bool areImageProperties;
    bool exportMemory;
    bool isSharedHandle;
    bool relaxedSizeAllowed;
    bool compressedHint;
    bool uncompressedHint;
    bool rayTracingMemory;
    bool bindlessImage;
    bool sampledImage;
};

inline ze_result_t prepareL0StructuresLookupTable(StructuresLookupTable &lookupTable, const void *desc) {
    const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(desc);
    while (extendedDesc) {
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_IMAGE_DESC) {
            const ze_image_desc_t *imageDesc = reinterpret_cast<const ze_image_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.imageDescriptor = convertDescriptor(*imageDesc);
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD) {
            lookupTable.isSharedHandle = true;
            const ze_external_memory_import_fd_t *linuxExternalMemoryImportDesc = reinterpret_cast<const ze_external_memory_import_fd_t *>(extendedDesc);
            if (linuxExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                lookupTable.sharedHandleType.isSupportedHandle = true;
                lookupTable.sharedHandleType.isOpaqueFDHandle = true;
                lookupTable.sharedHandleType.fd = linuxExternalMemoryImportDesc->fd;
            } else if (linuxExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF) {
                lookupTable.sharedHandleType.isSupportedHandle = true;
                lookupTable.sharedHandleType.isDMABUFHandle = true;
                lookupTable.sharedHandleType.fd = linuxExternalMemoryImportDesc->fd;
            } else {
                lookupTable.sharedHandleType.isSupportedHandle = false;
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32) {
            lookupTable.isSharedHandle = true;
            const ze_external_memory_import_win32_handle_t *windowsExternalMemoryImportDesc = reinterpret_cast<const ze_external_memory_import_win32_handle_t *>(extendedDesc);
            if ((windowsExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32) ||
                (windowsExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP) ||
                (windowsExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE) ||
                (windowsExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE)) {
                lookupTable.sharedHandleType.isSupportedHandle = true;
                lookupTable.sharedHandleType.isNTHandle = true;
                lookupTable.sharedHandleType.ntHandle = windowsExternalMemoryImportDesc->handle;
            } else {
                lookupTable.sharedHandleType.isSupportedHandle = false;
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC) {
            const ze_image_view_planar_exp_desc_t *imageViewDesc = reinterpret_cast<const ze_image_view_planar_exp_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.isPlanarExtension = true;
            lookupTable.imageProperties.planeIndex = imageViewDesc->planeIndex;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXT_DESC) {
            const ze_image_view_planar_ext_desc_t *imageViewDesc = reinterpret_cast<const ze_image_view_planar_ext_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.isPlanarExtension = true;
            lookupTable.imageProperties.planeIndex = imageViewDesc->planeIndex;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC) {
            const ze_image_bindless_exp_desc_t *imageBindlessDesc =
                reinterpret_cast<const ze_image_bindless_exp_desc_t *>(extendedDesc);
            lookupTable.bindlessImage = imageBindlessDesc->flags & ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;
            lookupTable.sampledImage = imageBindlessDesc->flags & ZE_IMAGE_BINDLESS_EXP_FLAG_SAMPLED_IMAGE;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC) {
            const ze_image_pitched_exp_desc_t *pitchedDesc = reinterpret_cast<const ze_image_pitched_exp_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.pitchedPtr = pitchedDesc->ptr;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_SAMPLER_DESC) {
            const ze_sampler_desc_t *samplerDesc = reinterpret_cast<const ze_sampler_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.samplerDesc = samplerDesc;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC) {
            const ze_relaxed_allocation_limits_exp_desc_t *relaxedLimitsDesc =
                reinterpret_cast<const ze_relaxed_allocation_limits_exp_desc_t *>(extendedDesc);
            if (!(relaxedLimitsDesc->flags & ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            lookupTable.relaxedSizeAllowed = true;
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC) {
            const ze_external_memory_export_desc_t *externalMemoryExportDesc =
                reinterpret_cast<const ze_external_memory_export_desc_t *>(extendedDesc);
            if (externalMemoryExportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF ||
                externalMemoryExportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD ||
                externalMemoryExportDesc->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32) {
                lookupTable.exportMemory = true;
            } else {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC) {
            auto memoryCompressionHintsDesc = reinterpret_cast<const ze_memory_compression_hints_ext_desc_t *>(extendedDesc);

            if (memoryCompressionHintsDesc->flags == ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED) {
                lookupTable.compressedHint = true;
            } else if (memoryCompressionHintsDesc->flags == ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED) {
                lookupTable.uncompressedHint = true;
            } else {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RAYTRACING_MEM_ALLOC_EXT_DESC) {
            lookupTable.rayTracingMemory = true;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }

        extendedDesc = reinterpret_cast<const ze_base_desc_t *>(extendedDesc->pNext);
    }

    if (lookupTable.areImageProperties && lookupTable.exportMemory) {
        return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
    }

    return ZE_RESULT_SUCCESS;
}

inline std::optional<NEO::SynchronizedDispatchMode> getSyncDispatchMode(const ze_base_desc_t *desc) {
    if (static_cast<uint32_t>(desc->stype) == ZE_STRUCTURE_TYPE_SYNCHRONIZED_DISPATCH_EXP_DESC) {
        auto syncDispatch = reinterpret_cast<const ze_synchronized_dispatch_exp_desc_t *>(desc);
        return (syncDispatch->flags == ZE_SYNCHRONIZED_DISPATCH_ENABLED_EXP_FLAG ? NEO::SynchronizedDispatchMode::full : NEO::SynchronizedDispatchMode::limited);
    }

    return std::nullopt;
}
} // namespace L0
