/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/mem_obj/mem_obj.h"

namespace NEO {

cl_int Context::processExtraProperties(cl_context_properties propertyType, cl_context_properties propertyValue) {
    return CL_INVALID_PROPERTY;
}

BlitOperationResult Context::blitMemoryToAllocation(MemObj &memObj, GraphicsAllocation *memory, void *hostPtr, size_t size) const {
    return BlitOperationResult::Unsupported;
}
} // namespace NEO
