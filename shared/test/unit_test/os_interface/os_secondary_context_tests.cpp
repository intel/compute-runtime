/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"

#include "gtest/gtest.h"

using namespace NEO;

struct SecondaryContextsTest : ::testing::Test {
    ExecutionEnvironment *execEnv;
    std::unique_ptr<MockDevice> device;

    void SetUp() override {
        execEnv = new ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(1);
        auto rootEnv = execEnv->rootDeviceEnvironments[0].get();
        rootEnv->setHwInfoAndInitHelpers(defaultHwInfo.get());
        execEnv->initializeMemoryManager();
        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), execEnv, 0));
        ASSERT_NE(nullptr, device);

        auto &gfxCoreHelper = rootEnv->getHelper<GfxCoreHelper>();
        if (!gfxCoreHelper.areSecondaryContextsSupported()) {
            GTEST_SKIP();
        }
    }

    bool getRegularSecondary(EngineGroupType groupType, SecondaryContexts *&secondaryPool) {
        auto group = device->tryGetRegularEngineGroup(groupType);
        if (!group || group->engines.empty()) {
            return false;
        }
        auto engineType = group->engines[0].osContext->getEngineType();
        auto it = device->secondaryEngines.find(engineType);
        if (it == device->secondaryEngines.end()) {
            secondaryPool = nullptr;
            return false;
        }
        secondaryPool = &it->second;
        return true;
    }
};

TEST_F(SecondaryContextsTest, givenRegularContextsWhenRotatingThenPrimaryEngineIsIncluded) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->regularEnginesTotal, 0u);

    // Populate npIndices
    for (uint32_t i = 0; i < secondary->regularEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
    }

    // Reuse path should eventually return primary
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    bool primaryIncluded = false;
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
        if (ec->osContext->getIsPrimaryEngine()) {
            primaryIncluded = true;
            break;
        }
    }
    EXPECT_TRUE(primaryIncluded);
}

TEST_F(SecondaryContextsTest, givenMatchingPriorityContextWhenReusingThenItIsPreferred) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->regularEnginesTotal, 0u);

    // Populate npIndices
    for (uint32_t i = 0; i < secondary->regularEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
    }

    // Mark one reused context with priority=2
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    EngineControl *marked = nullptr;
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
        if (!ec->osContext->getIsPrimaryEngine()) {
            marked = ec;
            break;
        }
        marked = ec;
    }
    ASSERT_NE(nullptr, marked);
    // Ensure it is the only one with priority 2
    marked->osContext->overridePriority(2);

    auto ecMatch = secondary->getEngine(EngineUsage::regular, 2);
    ASSERT_NE(nullptr, ecMatch);
    EXPECT_EQ(2, ecMatch->osContext->getPriorityLevel());
}

TEST_F(SecondaryContextsTest, givenUnsetPriorityContextWhenRequestingNewPriorityThenItIsOverridden) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->regularEnginesTotal, 0u);

    // Populate npIndices
    for (uint32_t i = 0; i < secondary->regularEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
    }

    // Ensure at least one context has no priority set
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    EngineControl *noPriorityCtx = nullptr;
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
        // Do not set any priority on purpose; find a context with no priority
        if (!ec->osContext->hasPriorityLevel()) {
            noPriorityCtx = ec;
            break;
        }
    }
    ASSERT_NE(nullptr, noPriorityCtx);

    const int requestedPriority = 1;
    auto ec = secondary->getEngine(EngineUsage::regular, requestedPriority);
    ASSERT_NE(nullptr, ec);
    // Selected context should now have the requested priority (it was unset before)
    EXPECT_TRUE(ec->osContext->hasPriorityLevel());
    EXPECT_EQ(requestedPriority, ec->osContext->getPriorityLevel());
}

TEST_F(SecondaryContextsTest, givenSetPriorityContextsWhenRequestingDifferentPriorityThenDoNotOverride) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->regularEnginesTotal, 0u);

    // Populate and set a default priority (e.g. 1) on all contexts
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::regular, std::nullopt);
        ASSERT_NE(nullptr, ec);
        if (!ec->osContext->hasPriorityLevel()) {
            ec->osContext->overridePriority(1);
        }
    }

    // Request a priority that does not exist; expect selection but no forced change
    const int requestedPriority = 2;
    auto ec = secondary->getEngine(EngineUsage::regular, requestedPriority);
    ASSERT_NE(nullptr, ec);
    EXPECT_NE(requestedPriority, ec->osContext->getPriorityLevel()); // should remain as previously set
}

TEST_F(SecondaryContextsTest, givenHighPriorityPoolWhenRequestingThenContextsAreInitializedAndReused) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->highPriorityEnginesTotal, 0u);

    // Request and initialize all high-priority contexts
    for (uint32_t i = 0; i < secondary->highPriorityEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::highPriority, std::nullopt);
        ASSERT_NE(nullptr, ec);
        EXPECT_EQ(EngineUsage::highPriority, ec->osContext->getEngineUsage());
    }

    // Reuse initialized contexts
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::highPriority, std::nullopt);
        ASSERT_NE(nullptr, ec);
        EXPECT_EQ(EngineUsage::highPriority, ec->osContext->getEngineUsage());
    }
}

TEST_F(SecondaryContextsTest, givenMatchingPriorityInHighPriorityPoolWhenRequestingThenItIsSelected) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->highPriorityEnginesTotal, 0u);

    // Populate HP indices
    for (uint32_t i = 0; i < secondary->highPriorityEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::highPriority, std::nullopt);
        ASSERT_NE(nullptr, ec);
    }

    // Mark one context with priority=1
    auto contextsCount = device->getGfxCoreHelper().getContextGroupContextsCount();
    EngineControl *marked = nullptr;
    for (uint32_t i = 0; i < contextsCount; i++) {
        auto ec = secondary->getEngine(EngineUsage::highPriority, std::nullopt);
        ASSERT_NE(nullptr, ec);
        if (!ec->osContext->getIsPrimaryEngine()) {
            marked = ec;
            break;
        }
    }
    ASSERT_NE(nullptr, marked);
    marked->osContext->overridePriority(1);

    // Request a high-priority engine with priority=1
    auto ecMatch = secondary->getEngine(EngineUsage::highPriority, 1);
    ASSERT_NE(nullptr, ecMatch);
    EXPECT_EQ(1, ecMatch->osContext->getPriorityLevel());
}

TEST_F(SecondaryContextsTest, givenNoMatchingPriorityInHighPriorityPoolWhenRequestingThenFallsBackToDefault) {
    SecondaryContexts *secondary = nullptr;
    ASSERT_TRUE(getRegularSecondary(EngineGroupType::compute, secondary));
    ASSERT_GT(secondary->highPriorityEnginesTotal, 0u);

    // Populate HP indices
    for (uint32_t i = 0; i < secondary->highPriorityEnginesTotal; i++) {
        auto ec = secondary->getEngine(EngineUsage::highPriority, std::nullopt);
        ASSERT_NE(nullptr, ec);
        ec->osContext->overridePriority(1); // Set all contexts to priority=1
    }

    // Request a high-priority engine with priority=2 (no match)
    auto ec = secondary->getEngine(EngineUsage::highPriority, 2);
    ASSERT_NE(nullptr, ec);
    EXPECT_EQ(2, ec->osContext->getPriorityLevel());
}

TEST(SecondaryContextsTests, givenOsContextWithNoPriorityLevelWhenRequestingEngineWithASpecificPriorityLevelThenSetPriorityLevelForOsContext) {
    SecondaryContexts secondaryContexts;
    EngineControl engineControl{nullptr, nullptr};

    // Prepare a mock OsContext with no specific priority level
    EngineDescriptor engineDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}, 1, PreemptionMode::Disabled, false);
    auto osContext = std::make_unique<MockOsContext>(0, engineDescriptor);
    EXPECT_FALSE(osContext->hasPriorityLevel());

    // Attach the OsContext to an EngineControl
    engineControl.osContext = osContext.get();
    secondaryContexts.engines.push_back(engineControl);
    secondaryContexts.regularEnginesTotal = 1;

    // Request engine with a specific priorityLevel
    std::optional<int> requestedPriority = 1;
    auto result = secondaryContexts.getEngine(EngineUsage::regular, requestedPriority);
    EXPECT_EQ(result, &secondaryContexts.engines[0]);
    EXPECT_EQ(1, osContext->getPriorityLevel());
}
