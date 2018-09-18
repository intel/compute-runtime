/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace PageTableEntry {
const uint32_t presentBit = 0;
const uint32_t writableBit = 1;
const uint32_t userSupervisorBit = 2;
} // namespace PageTableEntry
