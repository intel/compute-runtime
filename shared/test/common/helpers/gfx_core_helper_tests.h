/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct ComputeSlmTestInput {
    uint32_t expected;
    uint32_t slmSize;
};

using GfxCoreHelperTest = Test<DeviceFixture>;

struct GfxCoreHelperTestWithEnginesCheck : public GfxCoreHelperTest {
  private:
    static constexpr uint32_t engineArraySize = static_cast<uint32_t>(aub_stream::EngineType::NUM_ENGINES) * static_cast<uint32_t>(EngineUsage::engineUsageCount);
    std::array<uint8_t, engineArraySize> engineArray{};
    uint32_t checkedEngines = 0u;

  public:
    uint8_t getEngineCount(aub_stream::EngineType engineType, EngineUsage engineUsage) {
        auto engineTypeIdx = static_cast<uint32_t>(engineType);
        auto engineUsageIdx = static_cast<uint32_t>(engineUsage);
        auto enginesCount = engineArray[static_cast<uint32_t>(EngineUsage::engineUsageCount) * engineTypeIdx + engineUsageIdx];
        checkedEngines = checkedEngines - enginesCount;
        return enginesCount;
    }
    void countEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
        auto engineTypeIdx = static_cast<uint32_t>(engineType);
        auto engineUsageIdx = static_cast<uint32_t>(engineUsage);
        engineArray[static_cast<uint32_t>(EngineUsage::engineUsageCount) * engineTypeIdx + engineUsageIdx]++;
        checkedEngines++;
    }
    bool allEnginesChecked() {
        return 0u == checkedEngines;
    }
    void resetEngineCounters() {
        std::fill(engineArray.begin(), engineArray.end(), 0u);
        checkedEngines = 0u;
    }
};