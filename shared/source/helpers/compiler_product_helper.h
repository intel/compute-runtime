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
    virtual bool isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSplitMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSubgroupLocalBlockIoSupported() const = 0;
    virtual bool isDotAccumulateSupported() const = 0;
    virtual bool isCreateBufferWithPropertiesSupported() const = 0;
    virtual bool isSubgroupNamedBarrierSupported() const = 0;
    virtual bool isSubgroupExtendedBlockReadSupported() const = 0;
    virtual bool isForceToStatelessRequired() const = 0;
    virtual bool failBuildProgramWithStatefulAccessPreference() const = 0;
    virtual bool isDotIntegerProductExtensionSupported() const = 0;
    virtual void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const = 0;
    virtual const char *getCachingPolicyOptions(bool isDebuggerActive) const = 0;
    virtual uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getNumThreadsPerEu() const = 0;
    virtual std::string getDeviceExtensions(const HardwareInfo &hwInfo) const = 0;

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
    bool isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const override;
    bool isSplitMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const override;
    bool isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const override;
    bool isSubgroupLocalBlockIoSupported() const override;
    bool isDotAccumulateSupported() const override;
    bool isCreateBufferWithPropertiesSupported() const override;
    bool isSubgroupNamedBarrierSupported() const override;
    bool isSubgroupExtendedBlockReadSupported() const override;
    bool isForceToStatelessRequired() const override;
    bool failBuildProgramWithStatefulAccessPreference() const override;
    bool isDotIntegerProductExtensionSupported() const override;
    void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const override;
    const char *getCachingPolicyOptions(bool isDebuggerActive) const override;
    uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const override;
    uint32_t getNumThreadsPerEu() const override;
    std::string getDeviceExtensions(const HardwareInfo &hwInfo) const override;

    ~CompilerProductHelperHw() override = default;

  protected:
    CompilerProductHelperHw() = default;
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableCompilerProductHelper {

    EnableCompilerProductHelper() {
        auto compilerProductHelperCreateFunction = CompilerProductHelperHw<gfxProduct>::create;
        compilerProductHelperFactory[gfxProduct] = compilerProductHelperCreateFunction;
    }
};

} // namespace NEO
