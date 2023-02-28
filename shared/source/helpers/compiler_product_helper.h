/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/product_config_helper.h"

#include "igfxfmid.h"

#include <memory>

namespace NEO {

class CompilerProductHelper;
struct HardwareInfo;

using CompilerProductHelperCreateFunctionType = std::unique_ptr<CompilerProductHelper> (*)();
extern CompilerProductHelperCreateFunctionType compilerProductHelperFactory[IGFX_MAX_PRODUCT];

class CompilerProductHelper {
  public:
    static std::unique_ptr<CompilerProductHelper> create(PRODUCT_FAMILY product) {
        auto createFunction = compilerProductHelperFactory[product];
        if (createFunction == nullptr) {
            return nullptr;
        }
        auto compilerProductHelper = createFunction();
        return compilerProductHelper;
    }

    virtual bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isForceEmuInt32DivRemSPRequired() const = 0;
    virtual bool isStatelessToStatefulBufferOffsetSupported() const = 0;
    virtual bool isForceToStatelessRequired() const = 0;
    virtual bool failBuildProgramWithStatefulAccessPreference() const = 0;
    virtual void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const = 0;
    virtual const char *getCachingPolicyOptions(bool isDebuggerActive) const = 0;
    virtual uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const = 0;

    virtual ~CompilerProductHelper() = default;

  protected:
    CompilerProductHelper() = default;
};

template <PRODUCT_FAMILY gfxProduct>
class CompilerProductHelperHw : public CompilerProductHelper {
  public:
    static std::unique_ptr<CompilerProductHelper> create() {
        auto compilerProductHelper = std::unique_ptr<CompilerProductHelper>(new CompilerProductHelperHw());
        return compilerProductHelper;
    }

    bool isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) const override;
    bool isForceEmuInt32DivRemSPRequired() const override;
    bool isStatelessToStatefulBufferOffsetSupported() const override;
    bool isForceToStatelessRequired() const override;
    bool failBuildProgramWithStatefulAccessPreference() const override;
    void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const override;
    const char *getCachingPolicyOptions(bool isDebuggerActive) const override;
    uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const override;

    ~CompilerProductHelperHw() override = default;

  protected:
    CompilerProductHelperHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableCompilerProductHelper {

    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    EnableCompilerProductHelper() {
        auto compilerProductHelperCreateFunction = CompilerProductHelperHw<gfxProduct>::create;
        compilerProductHelperFactory[gfxProduct] = compilerProductHelperCreateFunction;
    }
};

} // namespace NEO
