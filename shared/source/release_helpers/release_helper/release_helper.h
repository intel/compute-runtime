/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/definitions/encode_size_preferred_slm_value.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/stackvec.h"

#include "supported_num_grfs.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {

class ReleaseHelper;
struct HardwareInfo;
enum class ReleaseType;
struct HardwareInfo;

inline constexpr uint32_t maxArchitecture = 64;
using createReleaseHelperFunctionType = std::unique_ptr<ReleaseHelper> (*)(HardwareIpVersion hardwareIpVersion);
inline constinit createReleaseHelperFunctionType *releaseHelperFactory[maxArchitecture]{};

using ThreadsPerEUConfigs = StackVec<uint32_t, 6>;
using SizeToPreferredSlmValueArray = std::array<SizeToPreferredSlmValue, 25>;

class ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion);
    virtual ~ReleaseHelper() = default;

    virtual bool isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(const HardwareInfo &hwInfo, bool isRcs) const = 0;
    virtual bool isResolvingSubDeviceIDNeeded() const = 0;
    virtual const SupportedNumGrfs getSupportedNumGrfs() const = 0;
    virtual uint64_t getTotalMemBankSize() const = 0;
    virtual const ThreadsPerEUConfigs getThreadsPerEUConfigs(uint32_t numThreadsPerEu) const = 0;
    virtual uint32_t getStackSizePerRay() const = 0;
    virtual const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue() const = 0;
    virtual uint32_t computeSlmValues(uint32_t slmSize) const = 0;
    virtual uint32_t alignSlmSizePerThreadGroup(uint32_t slmSize) const = 0;
    virtual uint32_t adjustMaxThreadsPerEuCount(uint32_t maxThreadsPerEuCount, uint32_t grfCount) const = 0;
    virtual bool isStateCacheInvalidationWaRequired(bool isImmediateCmdList, bool kernelUsesImageOrSampler) const = 0;
    virtual uint64_t overrideSystemMemoryPatIndexBase(uint64_t patIndex) const = 0;
    uint64_t overrideSystemMemoryPatIndex(uint64_t patIndex) const;
    virtual uint32_t getIpVersionForGmm() const = 0;

  protected:
    ReleaseHelper(HardwareIpVersion hardwareIpVersion) : hardwareIpVersion(hardwareIpVersion) {}
    HardwareIpVersion hardwareIpVersion{};
};

template <ReleaseType releaseType>
class ReleaseHelperHw : public ReleaseHelper {
  public:
    ReleaseHelperHw(HardwareIpVersion hardwareIpVersion) : ReleaseHelper(hardwareIpVersion) {}
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion) {
        return std::make_unique<ReleaseHelperHw<releaseType>>(hardwareIpVersion);
    }

    bool isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(const HardwareInfo &hwInfo, bool isRcs) const override;
    bool isResolvingSubDeviceIDNeeded() const override;
    const SupportedNumGrfs getSupportedNumGrfs() const override;
    uint64_t getTotalMemBankSize() const override;
    const StackVec<uint32_t, 6> getThreadsPerEUConfigs(uint32_t numThreadsPerEu) const override;
    uint32_t getStackSizePerRay() const override;
    const SizeToPreferredSlmValueArray &getSizeToPreferredSlmValue() const override;
    uint32_t computeSlmValues(uint32_t slmSize) const override;
    uint32_t alignSlmSizePerThreadGroup(uint32_t slmSize) const override;
    uint32_t adjustMaxThreadsPerEuCount(uint32_t maxThreadsPerEuCount, uint32_t grfCount) const override;
    bool isStateCacheInvalidationWaRequired(bool isImmediateCmdList, bool kernelUsesImageOrSampler) const override;
    uint64_t overrideSystemMemoryPatIndexBase(uint64_t patIndex) const override;
    uint32_t getIpVersionForGmm() const override;
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
