/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
extern ProductHelper *productHelperFactory[IGFX_MAX_PRODUCT];

template <typename MockProductHelper>
class RAIIProductHelperFactory {
  public:
    PRODUCT_FAMILY productFamily;
    ProductHelper *productHelper;
    MockProductHelper mockProductHelper{};

    RAIIProductHelperFactory(PRODUCT_FAMILY productFamily) {
        this->productFamily = productFamily;
        productHelper = productHelperFactory[this->productFamily];
        productHelperFactory[this->productFamily] = &mockProductHelper;
    }

    ~RAIIProductHelperFactory() {
        productHelperFactory[this->productFamily] = productHelper;
    }
};
} // namespace NEO