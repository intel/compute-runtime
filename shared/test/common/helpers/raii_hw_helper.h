/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"

namespace NEO {
extern GfxCoreHelper *gfxCoreHelperFactory[IGFX_MAX_CORE];

template <class MockHelper>
class RAIIGfxCoreHelperFactory {
  public:
    GFXCORE_FAMILY gfxCoreFamily;
    GfxCoreHelper *gfxCoreHelper;
    MockHelper mockGfxCoreHelper{};

    RAIIGfxCoreHelperFactory(GFXCORE_FAMILY gfxCoreFamily) {
        this->gfxCoreFamily = gfxCoreFamily;
        gfxCoreHelper = gfxCoreHelperFactory[this->gfxCoreFamily];
        gfxCoreHelperFactory[this->gfxCoreFamily] = &mockGfxCoreHelper;
    }

    ~RAIIGfxCoreHelperFactory() {
        gfxCoreHelperFactory[this->gfxCoreFamily] = gfxCoreHelper;
    }
};
} // namespace NEO