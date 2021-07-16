/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/surface_format_info.h"

#include <level_zero/ze_api.h>

#include <cstdint>

namespace L0 {
inline NEO::ImageType convertType(const ze_image_type_t type) {
    switch (type) {
    case ZE_IMAGE_TYPE_2D:
        return NEO::ImageType::Image2D;
    case ZE_IMAGE_TYPE_3D:
        return NEO::ImageType::Image3D;
    case ZE_IMAGE_TYPE_2DARRAY:
        return NEO::ImageType::Image2DArray;
    case ZE_IMAGE_TYPE_1D:
        return NEO::ImageType::Image1D;
    case ZE_IMAGE_TYPE_1DARRAY:
        return NEO::ImageType::Image1DArray;
    case ZE_IMAGE_TYPE_BUFFER:
        return NEO::ImageType::Image1DBuffer;
    default:
        break;
    }
    return NEO::ImageType::Invalid;
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
    bool isSharedHandle;
    struct SharedHandleType {
        bool isSupportedHandle;
        bool isDMABUFHandle;
        int fd;
        bool isNTHandle;
        void *ntHnadle;
    } sharedHandleType;
    bool areImageProperties;
    struct ImageProperties {
        bool isPlanarExtension;
        uint32_t planeIndex;
        NEO::ImageDescriptor imageDescriptor;
    } imageProperties;
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
            if (linuxExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF) {
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
            if (windowsExternalMemoryImportDesc->flags == ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32) {
                lookupTable.sharedHandleType.isSupportedHandle = true;
                lookupTable.sharedHandleType.isNTHandle = true;
                lookupTable.sharedHandleType.ntHnadle = windowsExternalMemoryImportDesc->handle;
            } else {
                lookupTable.sharedHandleType.isSupportedHandle = false;
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
        } else if (extendedDesc->stype == ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC) {
            const ze_image_view_planar_exp_desc_t *imageViewDesc = reinterpret_cast<const ze_image_view_planar_exp_desc_t *>(extendedDesc);
            lookupTable.areImageProperties = true;
            lookupTable.imageProperties.isPlanarExtension = true;
            lookupTable.imageProperties.planeIndex = imageViewDesc->planeIndex;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }

        extendedDesc = reinterpret_cast<const ze_base_desc_t *>(extendedDesc->pNext);
    }

    return ZE_RESULT_SUCCESS;
}
} // namespace L0
