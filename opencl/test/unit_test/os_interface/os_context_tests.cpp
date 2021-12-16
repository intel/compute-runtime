/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSContext, whenCreatingDefaultOsContextThenExpectInitializedAlways) {
    OsContext *osContext = OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_FALSE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenInternalAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    auto descriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal});
    descriptor.isRootDevice = true;

    OsContext *osContext = OsContext::create(nullptr, 0, descriptor);
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_TRUE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenLowPriorityAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    auto descriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::LowPriority});
    descriptor.isRootDevice = true;

    OsContext *osContext = OsContext::create(nullptr, 0, descriptor);
    EXPECT_TRUE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenOsContextCreatedDefaultIsFalseWhenSettingTrueThenFlagTrueReturned) {
    OsContext *osContext = OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_FALSE(osContext->isDefaultContext());
    osContext->setDefaultContext(true);
    EXPECT_TRUE(osContext->isDefaultContext());
    delete osContext;
}

TEST(OSContext, givenCooperativeEngineWhenIsCooperativeEngineIsCalledThenReturnTrue) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    engineDescriptor.engineTypeUsage.second = EngineUsage::Cooperative;
    auto pOsContext = OsContext::create(nullptr, 0, engineDescriptor);
    EXPECT_FALSE(pOsContext->isRegular());
    EXPECT_FALSE(pOsContext->isLowPriority());
    EXPECT_FALSE(pOsContext->isInternalEngine());
    EXPECT_TRUE(pOsContext->isCooperativeEngine());
    delete pOsContext;
}

TEST(OSContext, givenReinitializeContextWhenContextIsInitThenContextIsStillIinitializedAfter) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    auto pOsContext = OsContext::create(nullptr, 0, engineDescriptor);
    EXPECT_NO_THROW(pOsContext->reInitializeContext());
    EXPECT_NO_THROW(pOsContext->ensureContextInitialized());
    delete pOsContext;
}

TEST(OSContext, givenSetPowerHintThenGetPowerHintShowsTheSameValue) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    auto pOsContext = OsContext::create(nullptr, 0, engineDescriptor);
    pOsContext->setUmdPowerHintValue(1);
    EXPECT_EQ(1, pOsContext->getUmdPowerHintValue());
    delete pOsContext;
}

struct DeferredOsContextCreationTests : ::testing::Test {
    void SetUp() override {
        device = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
        DeviceFactory::prepareDeviceEnvironments(*device->getExecutionEnvironment());
    }

    std::unique_ptr<OsContext> createOsContext(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        OSInterface *osInterface = device->getRootDeviceEnvironment().osInterface.get();
        std::unique_ptr<OsContext> osContext{OsContext::create(osInterface, 0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage))};
        EXPECT_FALSE(osContext->isInitialized());
        return osContext;
    }

    void expectContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine, bool expectedImmediate) {
        auto osContext = createOsContext(engineTypeUsage, defaultEngine);
        const bool immediate = osContext->isImmediateContextInitializationEnabled(defaultEngine);
        EXPECT_EQ(expectedImmediate, immediate);
        if (immediate) {
            osContext->ensureContextInitialized();
            EXPECT_TRUE(osContext->isInitialized());
        }
    }

    void expectDeferredContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        expectContextCreation(engineTypeUsage, defaultEngine, false);
    }

    void expectImmediateContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        expectContextCreation(engineTypeUsage, defaultEngine, true);
    }

    std::unique_ptr<MockDevice> device;
    static inline const EngineTypeUsage engineTypeUsageRegular{aub_stream::ENGINE_RCS, EngineUsage::Regular};
    static inline const EngineTypeUsage engineTypeUsageInternal{aub_stream::ENGINE_RCS, EngineUsage::Internal};
    static inline const EngineTypeUsage engineTypeUsageBlitter{aub_stream::ENGINE_BCS, EngineUsage::Regular};
};

TEST_F(DeferredOsContextCreationTests, givenRegularEngineWhenCreatingOsContextThenOsContextIsInitializedDeferred) {
    DebugManagerStateRestore restore{};

    expectDeferredContextCreation(engineTypeUsageRegular, false);

    DebugManager.flags.DeferOsContextInitialization.set(1);
    expectDeferredContextCreation(engineTypeUsageRegular, false);

    DebugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageRegular, false);
}

TEST_F(DeferredOsContextCreationTests, givenDefaultEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageRegular, true);

    DebugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageRegular, true);

    DebugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageRegular, true);
}

TEST_F(DeferredOsContextCreationTests, givenInternalEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageInternal, false);

    DebugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageInternal, false);

    DebugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageInternal, false);
}

TEST_F(DeferredOsContextCreationTests, givenBlitterEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageBlitter, false);

    DebugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageBlitter, false);

    DebugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageBlitter, false);
}

TEST_F(DeferredOsContextCreationTests, givenEnsureContextInitializeCalledMultipleTimesWhenOsContextIsCreatedThenInitializeOnlyOnce) {
    struct MyOsContext : OsContext {
        MyOsContext(uint32_t contextId,
                    const EngineDescriptor &engineDescriptor) : OsContext(contextId, engineDescriptor) {}

        void initializeContext() override {
            initializeContextCalled++;
        }

        size_t initializeContextCalled = 0u;
    };

    MyOsContext osContext{0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsageRegular)};
    EXPECT_FALSE(osContext.isInitialized());

    osContext.ensureContextInitialized();
    EXPECT_TRUE(osContext.isInitialized());
    EXPECT_EQ(1u, osContext.initializeContextCalled);

    osContext.ensureContextInitialized();
    EXPECT_TRUE(osContext.isInitialized());
    EXPECT_EQ(1u, osContext.initializeContextCalled);
}

TEST_F(DeferredOsContextCreationTests, givenPrintOsContextInitializationsIsSetWhenOsContextItIsInitializedThenInfoIsLoggedToStdout) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.DeferOsContextInitialization.set(1);
    DebugManager.flags.PrintOsContextInitializations.set(1);
    testing::internal::CaptureStdout();

    auto osContext = createOsContext(engineTypeUsageRegular, false);
    EXPECT_EQ(std::string{}, testing::internal::GetCapturedStdout());

    testing::internal::CaptureStdout();
    osContext->ensureContextInitialized();
    std::string expectedMessage = "OsContext initialization: contextId=0 usage=Regular type=RCS isRootDevice=0\n";
    EXPECT_EQ(expectedMessage, testing::internal::GetCapturedStdout());
}
