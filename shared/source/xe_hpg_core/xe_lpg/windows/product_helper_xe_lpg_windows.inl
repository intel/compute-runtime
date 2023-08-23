/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}
} // namespace NEO
