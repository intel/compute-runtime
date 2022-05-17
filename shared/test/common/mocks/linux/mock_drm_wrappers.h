/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_wrappers.h"

namespace NEO {

struct MockExecObject : ExecObject {
    uint32_t getHandle() const;
    uint64_t getOffset() const;
    uint64_t getReserved() const;

    bool has48BAddressSupportFlag() const;
    bool hasCaptureFlag() const;
    bool hasAsyncFlag() const;
};
static_assert(sizeof(MockExecObject) == sizeof(ExecObject));

struct MockExecBuffer : ExecBuffer {
    uint64_t getBuffersPtr() const;
    uint32_t getBufferCount() const;
    uint32_t getBatchLen() const;
    uint32_t getBatchStartOffset() const;
    uint64_t getFlags() const;
    uint64_t getCliprectsPtr() const;
    uint64_t getReserved() const;

    bool hasUseExtensionsFlag() const;
};
static_assert(sizeof(MockExecBuffer) == sizeof(ExecBuffer));

} // namespace NEO
