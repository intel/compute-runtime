/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/allocator_helper.h"

#include "shared/source/helpers/basic_math.h"

namespace NEO {

size_t getSizeToReserve() {
    // 4 x sizeof(Heap32) + 2 x sizeof(Standard/Standard64k)
    return (4 * 4 + 2 * 4) * GB;
}

} // namespace NEO
