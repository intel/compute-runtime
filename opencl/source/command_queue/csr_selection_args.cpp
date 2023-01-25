/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/csr_selection_args.h"

#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"

namespace NEO {

void CsrSelectionArgs::processResource(const Image &image, uint32_t rootDeviceIndex, Resource &outResource) {
    processResource(image.getMultiGraphicsAllocation(), rootDeviceIndex, outResource);
    outResource.image = &image;
}

void CsrSelectionArgs::processResource(const Buffer &buffer, uint32_t rootDeviceIndex, Resource &outResource) {
    processResource(buffer.getMultiGraphicsAllocation(), rootDeviceIndex, outResource);
}

void CsrSelectionArgs::processResource(const MultiGraphicsAllocation &multiGfxAlloc, uint32_t rootDeviceIndex, Resource &outResource) {
    auto allocation = multiGfxAlloc.getGraphicsAllocation(rootDeviceIndex);
    if (allocation) {
        processResource(*allocation, rootDeviceIndex, outResource);
    }
}

void CsrSelectionArgs::processResource(const GraphicsAllocation &gfxAlloc, uint32_t rootDeviceIndex, Resource &outResource) {
    outResource.allocation = &gfxAlloc;
    outResource.isLocal = gfxAlloc.isAllocatedInLocalMemoryPool();
}

} // namespace NEO