/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/allocator_helper.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"

namespace OCLRT {
size_t getSizeToMap() {
    return static_cast<size_t>(alignUp(4 * GB - 8096, 4096));
}
} // namespace OCLRT
