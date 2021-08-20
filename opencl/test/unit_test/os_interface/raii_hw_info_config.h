/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
extern HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT];

template <typename MockHwInfoConfig>
class RAIIHwInfoConfigFactory {
  public:
    PRODUCT_FAMILY productFamily;
    HwInfoConfig *hwInfoConfig;
    MockHwInfoConfig mockHwInfoConfig{};

    RAIIHwInfoConfigFactory(PRODUCT_FAMILY productFamily) {
        this->productFamily = productFamily;
        hwInfoConfig = hwInfoConfigFactory[this->productFamily];
        hwInfoConfigFactory[this->productFamily] = &mockHwInfoConfig;
    }

    ~RAIIHwInfoConfigFactory() {
        hwInfoConfigFactory[this->productFamily] = hwInfoConfig;
    }
};
} // namespace NEO