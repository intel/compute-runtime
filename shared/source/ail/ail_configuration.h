/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"

#include <string>

#pragma once

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

enum class AILEnumeration : uint32_t {
    DISABLE_COMPRESSION,
    ENABLE_FP64,
    AIL_MAX_OPTIONS_COUNT
};

class AILConfiguration {
  public:
    AILConfiguration() = default;

    MOCKABLE_VIRTUAL bool initProcessExecutableName();

    static AILConfiguration *get(PRODUCT_FAMILY productFamily);

    virtual void apply(RuntimeCapabilityTable &runtimeCapabilityTable);

  protected:
    virtual void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) = 0;
    std::string processName;
};

extern AILConfiguration *ailConfigurationTable[IGFX_MAX_PRODUCT];

template <PRODUCT_FAMILY Product>
class AILConfigurationHw : public AILConfiguration {
  public:
    static AILConfigurationHw<Product> &get() {
        static AILConfigurationHw<Product> ailConfiguration;
        return ailConfiguration;
    }

    void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) override;
};

template <PRODUCT_FAMILY product>
struct EnableAIL {
    EnableAIL() {
        ailConfigurationTable[product] = &AILConfigurationHw<product>::get();
    }
};
} // namespace NEO
