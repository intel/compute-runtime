/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"

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
    virtual bool isForceEmuInt32DivRemSPRequired() const = 0;
    virtual bool isStatelessToStatefulBufferOffsetSupported() const = 0;
    virtual bool isForceToStatelessRequired() const = 0;
    virtual void setProductConfigForHwInfo(HardwareInfo &hwInfo, AheadOfTimeConfig config) const = 0;
    virtual const char *getCachingPolicyOptions(bool isDebuggerActive) const = 0;
};

template <PRODUCT_FAMILY gfxProduct>
class CompilerHwInfoConfigHw : public CompilerHwInfoConfig {
  public:
    static CompilerHwInfoConfig *get() {
        static CompilerHwInfoConfigHw<gfxProduct> instance;
        return &instance;
    }

    bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const override;
    bool isForceEmuInt32DivRemSPRequired() const override;
    bool isStatelessToStatefulBufferOffsetSupported() const override;
    bool isForceToStatelessRequired() const override;
    void setProductConfigForHwInfo(HardwareInfo &hwInfo, AheadOfTimeConfig config) const override;
    const char *getCachingPolicyOptions(bool isDebuggerActive) const override;

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
