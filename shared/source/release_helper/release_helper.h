/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/definitions/encode_size_preferred_slm_value.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {

class ReleaseHelper;
enum class ReleaseType;

inline constexpr uint32_t maxArchitecture = 64;
using createReleaseHelperFunctionType = std::unique_ptr<ReleaseHelper> (*)(HardwareIpVersion hardwareIpVersion);
inline createReleaseHelperFunctionType *releaseHelperFactory[maxArchitecture]{};

using ThreadsPerEUConfigs = StackVec<uint32_t, 6>;
using SizeToPreferredSlmValueArray = std::array<SizeToPreferredSlmValue, 25>;

class ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion);
    virtual ~ReleaseHelper() = default;

    virtual bool isAdjustWalkOrderAvailable() const = 0;
    virtual bool isMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isDotProductAccumulateSystolicSupported() const = 0;
    virtual bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const = 0;
    virtual bool isPipeControlPriorToPipelineSelectWaRequired() const = 0;
    virtual bool isProgramAllStateComputeCommandFieldsWARequired() const = 0;
    virtual bool isSplitMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isBFloat16ConversionSupported() const = 0;
    virtual bool isAuxSurfaceModeOverrideRequired() const = 0;
    virtual bool isResolvingSubDeviceIDNeeded() const = 0;
    virtual bool isDirectSubmissionSupported() const = 0;
    virtual bool isRcsExposureDisabled() const = 0;
    virtual std::vector<uint32_t> getSupportedNumGrfs() const = 0;
    virtual bool isBindlessAddressingDisabled() const = 0;
    virtual bool isGlobalBindlessAllocatorEnabled() const = 0;
    virtual uint64_t getTotalMemBankSize() const = 0;
    virtual const ThreadsPerEUConfigs getThreadsPerEUConfigs(uint32_t numThreadsPerEu) const = 0;
    virtual const std::string getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const = 0;
    virtual bool isRayTracingSupported() const = 0;
    virtual uint32_t getAdditionalFp16Caps() const = 0;
    virtual uint32_t getAdditionalExtraCaps() const = 0;
    virtual uint32_t getStackSizePerRay() const = 0;
    virtual void adjustRTDispatchGlobals(void *rtDispatchGlobals, uint32_t rtStacksPerDss, bool heaplessEnabled, uint32_t maxBvhLevels) const = 0;
    virtual bool isLocalOnlyAllowed() const = 0;
    virtual bool isDummyBlitWaRequired() const = 0;
    virtual bool isDirectSubmissionLightSupported() const = 0;
    virtual const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue(bool isHeapless) const = 0;
    virtual bool isNumRtStacksPerDssFixedValue() const = 0;
    virtual bool getFtrXe2Compression() const = 0;
    virtual bool programmAdditionalStallPriorToBarrierWithTimestamp() const = 0;
    virtual uint32_t computeSlmValues(uint32_t slmSize, bool isHeapless) const = 0;
    virtual bool isBlitImageAllowedForDepthFormat() const = 0;
    virtual bool isPostImageWriteFlushRequired() const = 0;
    virtual uint32_t adjustMaxThreadsPerEuCount(uint32_t maxThreadsPerEuCount, uint32_t grfCount) const = 0;
    virtual bool shouldQueryPeerAccess() const = 0;
    virtual bool isSpirSupported() const = 0;
    virtual bool isSingleDispatchRequiredForMultiCCS() const = 0;

  protected:
    ReleaseHelper(HardwareIpVersion hardwareIpVersion) : hardwareIpVersion(hardwareIpVersion) {}
    HardwareIpVersion hardwareIpVersion{};
};

template <ReleaseType releaseType>
class ReleaseHelperHw : public ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion) {
        return std::unique_ptr<ReleaseHelper>(new ReleaseHelperHw<releaseType>{hardwareIpVersion});
    }

    bool isAdjustWalkOrderAvailable() const override;
    bool isMatrixMultiplyAccumulateSupported() const override;
    bool isDotProductAccumulateSystolicSupported() const override;
    bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const override;
    bool isPipeControlPriorToPipelineSelectWaRequired() const override;
    bool isProgramAllStateComputeCommandFieldsWARequired() const override;
    bool isSplitMatrixMultiplyAccumulateSupported() const override;
    bool isBFloat16ConversionSupported() const override;
    bool isAuxSurfaceModeOverrideRequired() const override;
    bool isResolvingSubDeviceIDNeeded() const override;
    bool isDirectSubmissionSupported() const override;
    bool isRcsExposureDisabled() const override;
    std::vector<uint32_t> getSupportedNumGrfs() const override;
    bool isBindlessAddressingDisabled() const override;
    bool isGlobalBindlessAllocatorEnabled() const override;
    uint64_t getTotalMemBankSize() const override;
    const StackVec<uint32_t, 6> getThreadsPerEUConfigs(uint32_t numThreadsPerEu) const override;
    const std::string getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const override;
    bool isRayTracingSupported() const override;
    uint32_t getAdditionalFp16Caps() const override;
    uint32_t getAdditionalExtraCaps() const override;
    uint32_t getStackSizePerRay() const override;
    void adjustRTDispatchGlobals(void *rtDispatchGlobals, uint32_t rtStacksPerDss, bool heaplessEnabled, uint32_t maxBvhLevels) const override;
    bool isLocalOnlyAllowed() const override;
    bool isDummyBlitWaRequired() const override;
    bool isDirectSubmissionLightSupported() const override;
    const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue(bool isHeapless) const override;
    bool isNumRtStacksPerDssFixedValue() const override;
    bool getFtrXe2Compression() const override;
    bool programmAdditionalStallPriorToBarrierWithTimestamp() const override;
    uint32_t computeSlmValues(uint32_t slmSize, bool isHeapless) const override;
    bool isBlitImageAllowedForDepthFormat() const override;
    bool isPostImageWriteFlushRequired() const override;
    uint32_t adjustMaxThreadsPerEuCount(uint32_t maxThreadsPerEuCount, uint32_t grfCount) const override;
    bool shouldQueryPeerAccess() const override;
    bool isSpirSupported() const override;
    bool isSingleDispatchRequiredForMultiCCS() const override;

  protected:
    ReleaseHelperHw(HardwareIpVersion hardwareIpVersion) : ReleaseHelper(hardwareIpVersion) {}
};

template <uint32_t architecture>
struct EnableReleaseHelperArchitecture {
    EnableReleaseHelperArchitecture(createReleaseHelperFunctionType *releaseTable) {
        releaseHelperFactory[architecture] = releaseTable;
    }
};

template <ReleaseType releaseType>
struct EnableReleaseHelper {
    EnableReleaseHelper(createReleaseHelperFunctionType &releaseTableEntry) {
        using ReleaseHelperType = ReleaseHelperHw<releaseType>;
        releaseTableEntry = ReleaseHelperType::create;
    }
};

} // namespace NEO
