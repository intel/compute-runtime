/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/allocator_helper.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"

namespace NEO {

size_t getSizeToReserve() {
    return (maxNBitValue(47) + 1) / 4;
}

} // namespace NEO
