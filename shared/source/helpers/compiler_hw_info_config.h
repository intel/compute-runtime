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

class CompilerProductHelper;
struct HardwareInfo;
extern CompilerProductHelper *CompilerProductHelperFactory[IGFX_MAX_PRODUCT];

class CompilerProductHelper {
  public:
    static CompilerProductHelper *get(PRODUCT_FAMILY product) {
        return CompilerProductHelperFactory[product];
    }

    virtual bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isForceEmuInt32DivRemSPRequired() const = 0;
    virtual bool isStatelessToStatefulBufferOffsetSupported() const = 0;
    virtual bool isForceToStatelessRequired() const = 0;
    virtual void setProductConfigForHwInfo(HardwareInfo &hwInfo, AheadOfTimeConfig config) const = 0;
    virtual const char *getCachingPolicyOptions(bool isDebuggerActive) const = 0;
};

template <PRODUCT_FAMILY gfxProduct>
class CompilerProductHelperHw : public CompilerProductHelper {
  public:
    static CompilerProductHelper *get() {
        static CompilerProductHelperHw<gfxProduct> instance;
        return &instance;
    }

    bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const override;
    bool isForceEmuInt32DivRemSPRequired() const override;
    bool isStatelessToStatefulBufferOffsetSupported() const override;
    bool isForceToStatelessRequired() const override;
    void setProductConfigForHwInfo(HardwareInfo &hwInfo, AheadOfTimeConfig config) const override;
    const char *getCachingPolicyOptions(bool isDebuggerActive) const override;

  protected:
    CompilerProductHelperHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableCompilerProductHelper {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;

    EnableCompilerProductHelper() {
        CompilerProductHelper *pCompilerProductHelper = CompilerProductHelperHw<gfxProduct>::get();
        CompilerProductHelperFactory[gfxProduct] = pCompilerProductHelper;
    }
};

} // namespace NEO
