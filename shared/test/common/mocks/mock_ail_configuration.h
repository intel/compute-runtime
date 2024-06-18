/*
 * Copyright (C) 2023-2024 Intel Corporation
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

    bool isFallbackToPatchtokensRequired(const std::string &kernelSources) override {
        return false;
    }

    bool contextSyncFlagReturn = false;
    bool isContextSyncFlagRequired() override {
        return contextSyncFlagReturn;
    }

    bool isBufferPoolEnabledReturn = true;
    bool isBufferPoolEnabled() override {
        return isBufferPoolEnabledReturn;
    }

    bool fallbackToLegacyValidationLogic = false;
    bool useLegacyValidationLogic() override {
        return fallbackToLegacyValidationLogic;
    }
    bool forceRcsValue = false;
    bool forceRcs() override {
        return forceRcsValue;
    }

  protected:
    void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) override {}
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
