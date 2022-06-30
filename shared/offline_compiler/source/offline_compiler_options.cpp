/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>
#include <limits>

namespace NEO {
// AUB file folder location
const char *folderAUB = "aub_out";

// Initial value for HW tag
uint32_t initialHardwareTag = std::numeric_limits<uint32_t>::max();
} // namespace NEO
