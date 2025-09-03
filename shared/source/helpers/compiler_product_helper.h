/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_mapper.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/utilities/stackvec.h"

#include "neo_igfxfmid.h"
#include "ocl_igc_interface/code_type.h"

#include <memory>

namespace NEO {

class CompilerProductHelper;
struct HardwareInfo;
class ReleaseHelper;

struct OclCVersion {
    unsigned short major = 0;
    unsigned short minor = 0;
};

constexpr bool operator>(OclCVersion lhs, OclCVersion rhs) {
    return (lhs.major > rhs.major) || ((lhs.major == rhs.major) && (lhs.minor >= rhs.minor));
}

constexpr bool operator==(OclCVersion lhs, OclCVersion rhs) {
    return (lhs.major == rhs.major) && (lhs.minor == rhs.minor);
}

constexpr bool operator>=(OclCVersion lhs, OclCVersion rhs) {
    return (lhs > rhs) || (lhs == rhs);
}

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
    virtual bool isMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isMatrixMultiplyAccumulateTF32Supported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isSplitMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isBFloat16ConversionSupported(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isSubgroupLocalBlockIoSupported() const = 0;
    virtual bool isDotProductAccumulateSystolicSupported(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isCreateBufferWithPropertiesSupported() const = 0;
    virtual bool isSubgroupNamedBarrierSupported() const = 0;
    virtual bool isSubgroupExtendedBlockReadSupported() const = 0;
    virtual bool isSubgroup2DBlockIOSupported() const = 0;
    virtual bool isSubgroupBufferPrefetchSupported() const = 0;
    virtual bool isForceToStatelessRequired() const = 0;
    virtual bool failBuildProgramWithStatefulAccessPreference() const = 0;
    virtual bool isDotIntegerProductExtensionSupported() const = 0;
    virtual bool isSpirSupported(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool oclocEnforceZebinFormat() const = 0;
    virtual void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const = 0;
    virtual const char *getCachingPolicyOptions(bool isDebuggerActive) const = 0;
    virtual uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const = 0;
    virtual uint32_t getDefaultHwIpVersion() const = 0;
    virtual uint32_t matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const = 0;
    virtual std::string getDeviceExtensions(const HardwareInfo &hwInfo, const ReleaseHelper *releaseHelper) const = 0;
    virtual StackVec<OclCVersion, 5> getDeviceOpenCLCVersions(const HardwareInfo &hwInfo, OclCVersion max) const = 0;
    virtual void adjustHwInfoForIgc(HardwareInfo &hwInfo) const = 0;
    virtual bool isHeaplessModeEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isHeaplessStateInitEnabled(bool heaplessModeEnabled) const = 0;
    virtual void getKernelFp16AtomicCapabilities(const ReleaseHelper *releaseHelper, uint32_t &fp16Caps) const = 0;
    virtual void getKernelFp32AtomicCapabilities(uint32_t &fp32Caps) const = 0;
    virtual void getKernelFp64AtomicCapabilities(uint32_t &fp64Caps) const = 0;
    virtual void getKernelCapabilitiesExtra(const ReleaseHelper *releaseHelper, uint32_t &extraCaps) const = 0;
    virtual bool isBindlessAddressingDisabled(const ReleaseHelper *releaseHelper) const = 0;
    virtual bool isForceBindlessRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const char *getCustomIgcLibraryName() const = 0;
    virtual const char *getFinalizerLibraryName() const = 0;
    virtual bool useIgcAsFcl() const = 0;
    virtual IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation() const = 0;

    virtual ~CompilerProductHelper() = default;
    uint32_t getHwIpVersion(const HardwareInfo &hwInfo) const;

  protected:
    virtual uint32_t getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const = 0;
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
    bool isMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const override;
    bool isMatrixMultiplyAccumulateTF32Supported(const HardwareInfo &hwInfo) const override;
    bool isSplitMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const override;
    bool isBFloat16ConversionSupported(const ReleaseHelper *releaseHelper) const override;
    bool isSubgroupLocalBlockIoSupported() const override;
    bool isDotProductAccumulateSystolicSupported(const ReleaseHelper *releaseHelper) const override;
    bool isCreateBufferWithPropertiesSupported() const override;
    bool isSubgroupNamedBarrierSupported() const override;
    bool isSubgroupExtendedBlockReadSupported() const override;
    bool isSubgroup2DBlockIOSupported() const override;
    bool isSubgroupBufferPrefetchSupported() const override;
    bool isForceToStatelessRequired() const override;
    bool failBuildProgramWithStatefulAccessPreference() const override;
    bool isDotIntegerProductExtensionSupported() const override;
    bool isSpirSupported(const ReleaseHelper *releaseHelper) const override;
    bool oclocEnforceZebinFormat() const override;
    void setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const override;
    const char *getCachingPolicyOptions(bool isDebuggerActive) const override;
    uint64_t getHwInfoConfig(const HardwareInfo &hwInfo) const override;
    uint32_t getDefaultHwIpVersion() const override;
    uint32_t matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const override;
    std::string getDeviceExtensions(const HardwareInfo &hwInfo, const ReleaseHelper *releaseHelper) const override;
    StackVec<OclCVersion, 5> getDeviceOpenCLCVersions(const HardwareInfo &hwInfo, OclCVersion max) const override;
    void adjustHwInfoForIgc(HardwareInfo &hwInfo) const override;
    bool isHeaplessModeEnabled(const HardwareInfo &hwInfo) const override;
    bool isHeaplessStateInitEnabled(bool heaplessModeEnabled) const override;
    void getKernelFp16AtomicCapabilities(const ReleaseHelper *releaseHelper, uint32_t &fp16Caps) const override;
    void getKernelFp32AtomicCapabilities(uint32_t &fp32Caps) const override;
    void getKernelFp64AtomicCapabilities(uint32_t &fp64Caps) const override;
    void getKernelCapabilitiesExtra(const ReleaseHelper *releaseHelper, uint32_t &extraCaps) const override;
    bool isBindlessAddressingDisabled(const ReleaseHelper *releaseHelper) const override;
    bool isForceBindlessRequired(const HardwareInfo &hwInfo) const override;
    const char *getCustomIgcLibraryName() const override;
    const char *getFinalizerLibraryName() const override;
    bool useIgcAsFcl() const override;
    IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation() const override;

    ~CompilerProductHelperHw() override = default;

  protected:
    uint32_t getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const override;
    CompilerProductHelperHw();
};

template <PRODUCT_FAMILY gfxProduct>
struct EnableCompilerProductHelper {

    EnableCompilerProductHelper() {
        auto compilerProductHelperCreateFunction = CompilerProductHelperHw<gfxProduct>::create;
        compilerProductHelperFactory[gfxProduct] = compilerProductHelperCreateFunction;
    }
};

} // namespace NEO
