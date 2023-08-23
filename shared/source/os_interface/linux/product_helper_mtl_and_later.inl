/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isPlatformQuerySupported() const {
    return true;
}
} // namespace NEO
