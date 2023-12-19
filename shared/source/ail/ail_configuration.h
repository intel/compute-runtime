/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

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

struct RuntimeCapabilityTable;

enum class AILEnumeration : uint32_t {
    disableCompression,
    enableFp64,
    disableHostPtrTracking,
    enableLegacyPlatformName,
    disableDirectSubmission,
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

    virtual void apply(RuntimeCapabilityTable &runtimeCapabilityTable);

    virtual void modifyKernelIfRequired(std::string &kernel) = 0;

    virtual bool isFallbackToPatchtokensRequired(const std::string &kernelSources) = 0;

    virtual bool isContextSyncFlagRequired() = 0;

    virtual ~AILConfiguration() = default;

    virtual bool useLegacyValidationLogic() = 0;

  protected:
    virtual void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) = 0;
    std::string processName;

    bool sourcesContain(const std::string &sources, std::string_view contentToFind) const;
    MOCKABLE_VIRTUAL bool isKernelHashCorrect(const std::string &kernelSources, uint64_t expectedHash) const;
};

extern const std::set<std::string_view> applicationsContextSyncFlag;

template <PRODUCT_FAMILY product>
class AILConfigurationHw : public AILConfiguration {
  public:
    static std::unique_ptr<AILConfiguration> create() {
        auto ailConfiguration = std::unique_ptr<AILConfiguration>(new AILConfigurationHw());
        return ailConfiguration;
    }

    void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) override;

    void modifyKernelIfRequired(std::string &kernel) override;
    bool isFallbackToPatchtokensRequired(const std::string &kernelSources) override;
    bool isContextSyncFlagRequired() override;
    bool useLegacyValidationLogic() override;
};

template <PRODUCT_FAMILY product>
struct EnableAIL {
    EnableAIL() {
        auto ailConfigurationCreateFunction = AILConfigurationHw<product>::create;
        ailConfigurationFactory[product] = ailConfigurationCreateFunction;
    }
};

} // namespace NEO
