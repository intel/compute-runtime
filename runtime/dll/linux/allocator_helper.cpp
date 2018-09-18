/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/os_interface/linux/allocator_helper.h"

namespace OCLRT {
size_t getSizeToMap() {
    return static_cast<size_t>(alignUp(4 * GB - 8096, 4096));
}
} // namespace OCLRT
