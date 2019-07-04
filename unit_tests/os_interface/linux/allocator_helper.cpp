/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/allocator_helper.h"

#include "core/helpers/basic_math.h"

namespace NEO {

size_t getSizeToReserve() {
    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    return (4 * 4 + 2 * 4) * GB;
}

} // namespace NEO
