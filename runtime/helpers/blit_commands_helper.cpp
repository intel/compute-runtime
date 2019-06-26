/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/blit_commands_helper.h"

#include "runtime/helpers/timestamp_packet.h"

#include "CL/cl.h"

namespace NEO {
BlitProperties BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitWithHostPtrDirection copyDirection, Buffer *buffer, void *hostPtr, bool blocking, size_t offset, uint64_t copySize) {

    if (BlitterConstants::BlitWithHostPtrDirection::FromHostPtr == copyDirection) {
        return {nullptr, copyDirection, {}, buffer, nullptr, hostPtr, blocking, offset, 0, copySize};
    } else {
        return {nullptr, copyDirection, {}, nullptr, buffer, hostPtr, blocking, 0, offset, copySize};
    }
}

void BlitProperties::setHostPtrBuffer(Buffer *hostPtrBuffer) {
    if (BlitterConstants::BlitWithHostPtrDirection::FromHostPtr == copyDirection) {
        srcBuffer = hostPtrBuffer;
    } else {
        dstBuffer = hostPtrBuffer;
    }
}
} // namespace NEO
