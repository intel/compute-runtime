/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {
enum class TransferDirection {
    HostToHost,
    HostToLocal,
    LocalToHost,
    LocalToLocal,
};

struct CsrSelectionArgs {
    struct Resource {
        bool isLocal = false;
        const GraphicsAllocation *allocation = nullptr;
        const Image *image = nullptr;
        const size_t *imageOrigin = nullptr;
    };

    cl_command_type cmdType;
    const size_t *size = nullptr;
    Resource srcResource;
    Resource dstResource;
    TransferDirection direction;

    CsrSelectionArgs(cl_command_type cmdType, const size_t *size)
        : cmdType(cmdType),
          size(size),
          direction(TransferDirection::HostToHost) {}

    template <typename ResourceType>
    CsrSelectionArgs(cl_command_type cmdType, ResourceType *src, ResourceType *dst, uint32_t rootDeviceIndex, const size_t *size)
        : cmdType(cmdType),
          size(size) {
        if (src) {
            processResource(*src, rootDeviceIndex, this->srcResource);
        }
        if (dst) {
            processResource(*dst, rootDeviceIndex, this->dstResource);
        }
        this->direction = createTransferDirection(srcResource.isLocal, dstResource.isLocal);
    }

    CsrSelectionArgs(cl_command_type cmdType, Image *src, Image *dst, uint32_t rootDeviceIndex, const size_t *size, const size_t *srcOrigin, const size_t *dstOrigin)
        : CsrSelectionArgs(cmdType, src, dst, rootDeviceIndex, size) {
        if (src) {
            srcResource.imageOrigin = srcOrigin;
        }
        if (dst) {
            dstResource.imageOrigin = dstOrigin;
        }
    }

    static void processResource(const Image &image, uint32_t rootDeviceIndex, Resource &outResource) {
        processResource(image.getMultiGraphicsAllocation(), rootDeviceIndex, outResource);
        outResource.image = &image;
    }

    static void processResource(const Buffer &buffer, uint32_t rootDeviceIndex, Resource &outResource) {
        processResource(buffer.getMultiGraphicsAllocation(), rootDeviceIndex, outResource);
    }

    static void processResource(const MultiGraphicsAllocation &multiGfxAlloc, uint32_t rootDeviceIndex, Resource &outResource) {
        processResource(*multiGfxAlloc.getGraphicsAllocation(rootDeviceIndex), rootDeviceIndex, outResource);
    }

    static void processResource(const GraphicsAllocation &gfxAlloc, uint32_t rootDeviceIndex, Resource &outResource) {
        outResource.allocation = &gfxAlloc;
        outResource.isLocal = gfxAlloc.isAllocatedInLocalMemoryPool();
    }

    static inline TransferDirection createTransferDirection(bool srcLocal, bool dstLocal) {
        if (srcLocal) {
            if (dstLocal) {
                return TransferDirection::LocalToLocal;
            } else {
                return TransferDirection::LocalToHost;
            }
        } else {
            if (dstLocal) {
                return TransferDirection::HostToLocal;
            } else {
                return TransferDirection::HostToHost;
            }
        }
    }
};

} // namespace NEO
