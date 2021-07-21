/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"

#include <string>

#pragma once

namespace NEO {
enum class AILEnumeration : uint32_t {
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
