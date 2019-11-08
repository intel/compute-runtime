/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_buffer.h"

#include "runtime/context/context.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/helpers/get_info.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"

#include "config.h"

using namespace NEO;

Buffer *UnifiedBuffer::createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto graphicsAllocation = UnifiedBuffer::createGraphicsAllocation(context, extMem.handle);
    if (!graphicsAllocation) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    UnifiedSharingFunctions *sharingFunctions = context->getSharing<UnifiedSharingFunctions>();
    auto sharingHandler = new UnifiedBuffer(sharingFunctions, extMem.type);
    return Buffer::createSharedBuffer(context, flags, sharingHandler, graphicsAllocation);
}

GraphicsAllocation *UnifiedBuffer::createGraphicsAllocation(Context *context, void *handle) {
    auto graphicsAllocation = context->getMemoryManager()->createGraphicsAllocationFromNTHandle(handle, 0);
    return graphicsAllocation;
}
