/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForQueues(bool heaplessEnabled) const {
    return true;
}
} // namespace NEO
