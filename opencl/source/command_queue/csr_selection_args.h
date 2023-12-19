/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/transfer_direction.h"

#include "opencl/source/api/cl_types.h"

namespace NEO {
class MultiGraphicsAllocation;
class GraphicsAllocation;
class Image;
class Buffer;

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
          direction(TransferDirection::hostToHost) {}

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

    static void processResource(const Image &image, uint32_t rootDeviceIndex, Resource &outResource);

    static void processResource(const Buffer &buffer, uint32_t rootDeviceIndex, Resource &outResource);

    static void processResource(const MultiGraphicsAllocation &multiGfxAlloc, uint32_t rootDeviceIndex, Resource &outResource);

    static void processResource(const GraphicsAllocation &gfxAlloc, uint32_t rootDeviceIndex, Resource &outResource);
};

} // namespace NEO
