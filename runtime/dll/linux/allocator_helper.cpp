/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/allocator_helper.h"

#include "core/helpers/basic_math.h"
#include "runtime/helpers/aligned_memory.h"

namespace NEO {

size_t getSizeToReserve() {
    return (maxNBitValue<47> + 1) / 4;
}

} // namespace NEO
