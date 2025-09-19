/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "neo_igfxfmid.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>

/*
 * AIL (Application Intelligence Layer) is a set of per-application controls that influence driver behavior.
 * The primary goal is to improve user experience and/or performance.
 *
 * AIL provides application detection mechanism based on running processes in the system.
 * Mechanism works on Windows and Linux, is flexible and easily extendable to new applications.
 *
 * E.g. AIL can detect running Blender application and enable fp64 emulation on hardware
 * that does not support native fp64.
 *
 * Disclaimer: we should never use this for benchmarking or conformance purposes - this would be cheating.
 *
 */

namespace NEO {

extern const char legacyPlatformName[];

struct HardwareInfo;
struct RuntimeCapabilityTable;

enum class AILEnumeration : uint32_t {
    disableCompression,
    enableFp64,
    disableHostPtrTracking,
    enableLegacyPlatformName,
    disableDirectSubmission,
    disableDirectSubmissionBcs,
    directSubmissionControllerConfig,
    handleDivergentBarriers,
    disableBindlessAddressing,
    disableZeroContract,
    forceLocalOnlyForDeviceUSM,
    customWmtpDataSize,
};

class AILConfiguration;
using AILConfigurationCreateFunctionType = std::unique_ptr<AILConfiguration> (*)();
extern AILConfigurationCreateFunctionType ailConfigurationFactory[IGFX_MAX_PRODUCT];

class AILConfiguration {
  public:
    static std::unique_ptr<AILConfiguration> create(PRODUCT_FAMILY product) {
        auto ailConfigurationCreateFunction = ailConfigurationFactory[product];
        if (ailConfigurationCreateFunction == nullptr) {
            return nullptr;
        }
        auto ailConfiguration = ailConfigurationCreateFunction();
        return ailConfiguration;
    }
    AILConfiguration() = default;

    MOCKABLE_VIRTUAL bool initProcessExecutableName();

    virtual void apply(HardwareInfo &hwInfo);

    virtual void modifyKernelIfRequired(std::string &kernel) = 0;

    virtual bool isContextSyncFlagRequired() = 0;

    virtual bool is256BPrefetchDisableRequired() = 0;

    virtual bool drainHostptrs() = 0;

    virtual bool isBufferPoolEnabled() = 0;

    virtual ~AILConfiguration() = default;

    virtual bool useLegacyValidationLogic() = 0;

    virtual bool forceRcs() = 0;

    virtual bool handleDivergentBarriers() = 0;

    virtual bool disableBindlessAddressing() = 0;

    virtual bool limitAmountOfDeviceMemoryForRecycling() = 0;

    virtual bool isRunAloneContextRequired() = 0;

    virtual bool isAdjustMicrosecondResolutionRequired() = 0;

    virtual uint32_t getMicrosecondResolution() = 0;

  protected:
    virtual void applyExt(HardwareInfo &hwInfo) = 0;
    std::string processName;

    bool sourcesContain(const std::string &sources, std::string_view contentToFind) const;
    MOCKABLE_VIRTUAL bool isKernelHashCorrect(const std::string &kernelSources, uint64_t expectedHash) const;
    virtual void setHandleDivergentBarriers(bool val) = 0;
    virtual void setDisableBindlessAddressing(bool val) = 0;
};

extern const std::set<std::string_view> applicationsContextSyncFlag;
extern const std::set<std::string_view> applicationsForceRcsDg2;
extern const std::set<std::string_view> applicationsBufferPoolDisabled;
extern const std::set<std::string_view> applicationsBufferPoolDisabledXe;
extern const std::set<std::string_view> applicationsOverfetchDisabled;
extern const std::set<std::string_view> applicationsDrainHostptrsDisabled;
extern const std::set<std::string_view> applicationsDeviceUSMRecyclingLimited;
extern const std::set<std::string_view> applicationsMicrosecontResolutionAdjustment;

extern const uint32_t microsecondAdjustment;

template <PRODUCT_FAMILY product>
class AILConfigurationHw : public AILConfiguration {
  public:
    static std::unique_ptr<AILConfiguration> create() {
        auto ailConfiguration = std::unique_ptr<AILConfiguration>(new AILConfigurationHw());
        return ailConfiguration;
    }

    void applyExt(HardwareInfo &hwInfo) override;

    void modifyKernelIfRequired(std::string &kernel) override;
    bool isContextSyncFlagRequired() override;
    bool is256BPrefetchDisableRequired() override;
    bool drainHostptrs() override;
    bool isBufferPoolEnabled() override;
    bool useLegacyValidationLogic() override;
    bool forceRcs() override;
    bool handleDivergentBarriers() override;
    bool disableBindlessAddressing() override;
    bool limitAmountOfDeviceMemoryForRecycling() override;
    bool isRunAloneContextRequired() override;
    bool isAdjustMicrosecondResolutionRequired() override;
    uint32_t getMicrosecondResolution() override;

    bool shouldForceRcs = false;
    bool shouldHandleDivergentBarriers = false;
    bool shouldDisableBindlessAddressing = false;
    bool shouldAdjustMicrosecondResolution = false;

  protected:
    void setHandleDivergentBarriers(bool val) override;
    void setDisableBindlessAddressing(bool val) override;
};

template <PRODUCT_FAMILY product>
struct EnableAIL {
    EnableAIL() {
        auto ailConfigurationCreateFunction = AILConfigurationHw<product>::create;
        ailConfigurationFactory[product] = ailConfigurationCreateFunction;
    }
};

} // namespace NEO
