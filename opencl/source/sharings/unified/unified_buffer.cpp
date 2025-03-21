/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/unified_buffer.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"

using namespace NEO;

Buffer *UnifiedBuffer::createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem, cl_int *errcodeRet) {
    auto multiGraphicsAllocation = UnifiedBuffer::createMultiGraphicsAllocation(context, extMem, nullptr, AllocationType::sharedBuffer, errcodeRet);
    if (!multiGraphicsAllocation) {
        return nullptr;
    }

    auto sharingHandler = new UnifiedBuffer(context->getSharing<UnifiedSharingFunctions>(), extMem.type);

    return Buffer::createSharedBuffer(context, flags, sharingHandler, std::move(*multiGraphicsAllocation));
}
