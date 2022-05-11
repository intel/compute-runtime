/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {

struct MockExecObject : ExecObject {
    uint32_t getHandle() const;
    uint64_t getOffset() const;
    bool has48BAddressSupportFlag() const;
    bool hasCaptureFlag() const;
    bool hasAsyncFlag() const;
};

static_assert(sizeof(MockExecObject) == sizeof(ExecObject));
} // namespace NEO
