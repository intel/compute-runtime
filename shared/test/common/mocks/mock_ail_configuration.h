/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/ail/ail_configuration.h"

namespace NEO {
class MockAILConfiguration : public AILConfiguration {
  public:
    bool initProcessExecutableNameResult = true;
    bool initProcessExecutableName() override {
        return initProcessExecutableNameResult;
    }
    void modifyKernelIfRequired(std::string &kernel) override {}

    bool contextSyncFlagReturn = false;
    bool isContextSyncFlagRequired() override {
        return contextSyncFlagReturn;
    }

    bool prefetchDisableRequiredReturn = false;
    bool is256BPrefetchDisableRequired() override {
        return prefetchDisableRequiredReturn;
    }

    bool isBufferPoolEnabledReturn = true;
    bool isBufferPoolEnabled() override {
        return isBufferPoolEnabledReturn;
    }

    bool isRunAloneContextRequiredReturn = false;
    bool isRunAloneContextRequired() override {
        return isRunAloneContextRequiredReturn;
    }

    bool limitAmountOfDeviceMemoryForRecyclingReturn = false;
    bool limitAmountOfDeviceMemoryForRecycling() override {
        return limitAmountOfDeviceMemoryForRecyclingReturn;
    }

    bool fallbackToLegacyValidationLogic = false;
    bool useLegacyValidationLogic() override {
        return fallbackToLegacyValidationLogic;
    }
    bool forceRcsValue = false;
    bool forceRcs() override {
        return forceRcsValue;
    }

    bool handleDivergentBarriers() override {
        return handleDivergentBarriersValue;
    }
    void setHandleDivergentBarriers(bool val) override {
        handleDivergentBarriersValue = val;
    }
    bool handleDivergentBarriersValue = false;

    bool disableBindlessAddressing() override {
        return disableBindlessAddressingValue;
    }
    void setDisableBindlessAddressing(bool val) override {
        disableBindlessAddressingValue = val;
    }
    bool disableBindlessAddressingValue = false;

    bool drainHostptrs() override {
        return true;
    }

    bool isAdjustMicrosecondResolutionRequired() override {
        return adjustMicrosecondResolution;
    }
    bool adjustMicrosecondResolution = false;

    uint32_t getMicrosecondResolution() override {
        getMicrosecondResolutionCalledTimes++;
        return mockMicrosecondResolution;
    }
    uint32_t getMicrosecondResolutionCalledTimes = 0u;
    uint32_t mockMicrosecondResolution = 1000u;

  protected:
    void applyExt(HardwareInfo &hwInfo) override {}
};

template <PRODUCT_FAMILY productFamily>
class AILWhitebox : public AILConfigurationHw<productFamily> {
  public:
    using AILConfiguration::apply;
    using AILConfiguration::isKernelHashCorrect;
    using AILConfiguration::processName;
    using AILConfiguration::sourcesContain;
};

} // namespace NEO
