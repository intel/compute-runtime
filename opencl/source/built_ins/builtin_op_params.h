/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/transfer_direction.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/vec.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class GraphicsAllocation;
class MemObj;

namespace BuiltIn {

struct OpParams {
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;
    MemObj *srcMemObj = nullptr;
    MemObj *dstMemObj = nullptr;
    GraphicsAllocation *srcSvmAlloc = nullptr;
    GraphicsAllocation *dstSvmAlloc = nullptr;
    GraphicsAllocation *transferAllocation = nullptr; // mapAllocation or hostPtrAllocation
    AuxTranslationDirection auxTranslationDirection = AuxTranslationDirection::none;
    bool unifiedMemoryArgsRequireMemSync = true;
    Vec3<size_t> srcOffset = {0, 0, 0};
    Vec3<size_t> dstOffset = {0, 0, 0};
    Vec3<size_t> size = {0, 0, 0};
    size_t srcRowPitch = 0;
    size_t dstRowPitch = 0;
    size_t srcSlicePitch = 0;
    size_t dstSlicePitch = 0;
    uint32_t srcMipLevel = 0;
    uint32_t dstMipLevel = 0;
    void *userPtrForPostOperationCpuCopy = nullptr;
    bool bcsSplit = false;
    TransferDirection direction = TransferDirection::localToLocal;
};

} // namespace BuiltIn
} // namespace NEO
