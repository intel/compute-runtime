/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return true;
}

} // namespace NEO
