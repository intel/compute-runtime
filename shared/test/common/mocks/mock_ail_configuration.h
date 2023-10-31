/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/ail/ail_configuration.h"

namespace NEO {
class MockAILConfiguration : public AILConfiguration {
  public:
    bool initProcessExecutableName() override {
        return true;
    }
    void modifyKernelIfRequired(std::string &kernel) override {}

    bool isFallbackToPatchtokensRequired(const std::string &kernelSources) override {
        return false;
    }

    bool contextSyncFlagReturn = false;
    bool isContextSyncFlagRequired() override {
        return contextSyncFlagReturn;
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
