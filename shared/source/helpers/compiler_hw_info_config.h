/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"

#include "igfxfmid.h"

namespace NEO {

class CompilerHwInfoConfig;
struct HardwareInfo;
extern CompilerHwInfoConfig *CompilerHwInfoConfigFactory[IGFX_MAX_PRODUCT];

class CompilerHwInfoConfig {
  public:
    static CompilerHwInfoConfig *get(PRODUCT_FAMILY product) {
        return CompilerHwInfoConfigFactory[product];
    }

    virtual bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const = 0;
};

template <PRODUCT_FAMILY gfxProduct>
class CompilerHwInfoConfigHw : public CompilerHwInfoConfig {
  public:
    static CompilerHwInfoConfig *get() {
        static CompilerHwInfoConfigHw<gfxProduct> instance;
        return &instance;
    }

    bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const override;

  protected:
    CompilerHwInfoConfigHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableCompilerHwInfoConfig {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;

    EnableCompilerHwInfoConfig() {
        CompilerHwInfoConfig *pCompilerHwInfoConfig = CompilerHwInfoConfigHw<gfxProduct>::get();
        CompilerHwInfoConfigFactory[gfxProduct] = pCompilerHwInfoConfig;
    }
};

} // namespace NEO
