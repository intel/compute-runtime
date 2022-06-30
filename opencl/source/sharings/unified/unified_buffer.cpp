/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unified_buffer.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"

#include "config.h"

using namespace NEO;

Buffer *UnifiedBuffer::createSharedUnifiedBuffer(Context *context, cl_mem_flags flags, UnifiedSharingMemoryDescription extMem, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    auto graphicsAllocation = UnifiedBuffer::createGraphicsAllocation(context, extMem, AllocationType::SHARED_BUFFER);
    if (!graphicsAllocation) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    UnifiedSharingFunctions *sharingFunctions = context->getSharing<UnifiedSharingFunctions>();
    auto sharingHandler = new UnifiedBuffer(sharingFunctions, extMem.type);
    auto rootDeviceIndex = graphicsAllocation->getRootDeviceIndex();
    auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
    multiGraphicsAllocation.addAllocation(graphicsAllocation);

    return Buffer::createSharedBuffer(context, flags, sharingHandler, std::move(multiGraphicsAllocation));
}
