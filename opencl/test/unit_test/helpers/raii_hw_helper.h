/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"

namespace NEO {
extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

template <class MockHelper>
class RAIIHwHelperFactory {
  public:
    GFXCORE_FAMILY gfxCoreFamily;
    HwHelper *hwHelper;
    MockHelper mockHwHelper;

    RAIIHwHelperFactory(GFXCORE_FAMILY gfxCoreFamily) {
        this->gfxCoreFamily = gfxCoreFamily;
        hwHelper = hwHelperFactory[this->gfxCoreFamily];
        hwHelperFactory[this->gfxCoreFamily] = &mockHwHelper;
    }

    ~RAIIHwHelperFactory() {
        hwHelperFactory[this->gfxCoreFamily] = hwHelper;
    }
};
} // namespace NEO