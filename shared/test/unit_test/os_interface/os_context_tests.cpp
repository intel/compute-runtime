/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSContext, whenCreatingDefaultOsContextThenExpectInitializedAlways) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_FALSE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenInternalAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    auto descriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal});
    descriptor.isRootDevice = true;

    OsContext *osContext = OsContext::create(nullptr, 0, 0, descriptor);
    EXPECT_FALSE(osContext->isLowPriority());
    EXPECT_TRUE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenLowPriorityAndRootDeviceAreTrueWhenCreatingDefaultOsContextThenExpectGettersTrue) {
    auto descriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority});
    descriptor.isRootDevice = true;

    OsContext *osContext = OsContext::create(nullptr, 0, 0, descriptor);
    EXPECT_TRUE(osContext->isLowPriority());
    EXPECT_FALSE(osContext->isInternalEngine());
    EXPECT_TRUE(osContext->isRootDevice());
    delete osContext;
}

TEST(OSContext, givenOsContextCreatedDefaultIsFalseWhenSettingTrueThenFlagTrueReturned) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_FALSE(osContext->isDefaultContext());
    osContext->setDefaultContext(true);
    EXPECT_TRUE(osContext->isDefaultContext());
    delete osContext;
}

TEST(OSContext, givenCooperativeEngineWhenIsCooperativeEngineIsCalledThenReturnTrue) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    engineDescriptor.engineTypeUsage.second = EngineUsage::cooperative;
    auto pOsContext = OsContext::create(nullptr, 0, 0, engineDescriptor);
    EXPECT_FALSE(pOsContext->isRegular());
    EXPECT_FALSE(pOsContext->isLowPriority());
    EXPECT_FALSE(pOsContext->isInternalEngine());
    EXPECT_TRUE(pOsContext->isCooperativeEngine());
    delete pOsContext;
}

TEST(OSContext, givenReinitializeContextWhenContextIsInitThenContextIsStillIinitializedAfter) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    auto pOsContext = OsContext::create(nullptr, 0, 0, engineDescriptor);
    EXPECT_NO_THROW(pOsContext->reInitializeContext());
    EXPECT_NO_THROW(pOsContext->ensureContextInitialized(false));
    delete pOsContext;
}

TEST(OSContext, givenSetPowerHintThenGetPowerHintShowsTheSameValue) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    auto pOsContext = OsContext::create(nullptr, 0, 0, engineDescriptor);
    pOsContext->setUmdPowerHintValue(1);
    EXPECT_EQ(1, pOsContext->getUmdPowerHintValue());
    delete pOsContext;
}

TEST(OSContext, givenPowerHintSetToMaxWhenCheckingDirectSubmissionAvailabilityThenFalseIsReturnedUnlessDebugFlagEnableDirectSubmissionTrue) {
    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor();
    auto pOsContext = std::make_unique<OsContextMock>(0, 0, engineDescriptor);
    ASSERT_NE(pOsContext, nullptr);
    pOsContext->callBaseIsDirectSubmissionSupported = false;
    pOsContext->mockDirectSubmissionSupported = true;
    pOsContext->setDefaultContext(true);
    ASSERT_NE(defaultHwInfo, nullptr);
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.directSubmissionEngines.data[pOsContext->getEngineType()]
        .engineSupported = true;
    bool submitOnInit = false;

    pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax());
    EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
    pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax() / 2);
    EXPECT_TRUE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));

    DebugManagerStateRestore debugFlagsStateRestorer;
    {
        debugManager.flags.EnableDirectSubmission.set(-1); // only in this case second/csr flag have any significance here
        {
            debugManager.flags.SetCommandStreamReceiver.set(0); // as representation of values <= 0
            pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax());
            EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
            pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax() / 2);
            EXPECT_TRUE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
        }
        {
            debugManager.flags.SetCommandStreamReceiver.set(1); // as representation of values > 0
            pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax());
            EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
            pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax() / 2);
            EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
        }
    }
    {
        debugManager.flags.EnableDirectSubmission.set(0);
        pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax());
        EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
        pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax() / 2);
        EXPECT_FALSE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
    }
    {
        debugManager.flags.EnableDirectSubmission.set(1);
        pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax());
        EXPECT_TRUE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
        pOsContext->setUmdPowerHintValue(OsContextMock::getUmdPowerHintMax() / 2);
        EXPECT_TRUE(pOsContext->isDirectSubmissionAvailable(hwInfo, submitOnInit));
    }
}

TEST(OSContext, givenOsContextWhenQueryingForOfflineDumpContextIdThenCorrectValueIsReturned) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor());
    EXPECT_EQ(0u, osContext->getOfflineDumpContextId(0));
    delete osContext;
}

TEST(OSContext, givenOsContextWhenSetNewResourceBoundThenTlbFLushRequired) {
    auto osContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, 0, EngineDescriptorHelper::getDefaultDescriptor()));

    EXPECT_EQ(osContext->peekTlbFlushCounter(), 0u);
    EXPECT_FALSE(osContext->isTlbFlushRequired());

    osContext->setNewResourceBound();

    EXPECT_EQ(osContext->peekTlbFlushCounter(), 1u);
    EXPECT_TRUE(osContext->isTlbFlushRequired());

    osContext->setTlbFlushed(1u);

    EXPECT_EQ(osContext->peekTlbFlushCounter(), 1u);
    EXPECT_FALSE(osContext->isTlbFlushRequired());
}

struct DeferredOsContextCreationTests : ::testing::Test {
    void SetUp() override {
        device = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
        DeviceFactory::prepareDeviceEnvironments(*device->getExecutionEnvironment());
    }

    std::unique_ptr<OsContext> createOsContext(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        OSInterface *osInterface = device->getRootDeviceEnvironment().osInterface.get();
        std::unique_ptr<OsContext> osContext{OsContext::create(osInterface, device->getRootDeviceIndex(), 0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage))};
        EXPECT_FALSE(osContext->isInitialized());
        return osContext;
    }

    void expectContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine, bool expectedImmediate) {
        auto osContext = createOsContext(engineTypeUsage, defaultEngine);
        const bool immediate = osContext->isImmediateContextInitializationEnabled(defaultEngine);
        EXPECT_EQ(expectedImmediate, immediate);
        if (immediate) {
            osContext->ensureContextInitialized(false);
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
    static inline const EngineTypeUsage engineTypeUsageRegular{aub_stream::ENGINE_RCS, EngineUsage::regular};
    static inline const EngineTypeUsage engineTypeUsageInternal{aub_stream::ENGINE_RCS, EngineUsage::internal};
    static inline const EngineTypeUsage engineTypeUsageBlitter{aub_stream::ENGINE_BCS, EngineUsage::regular};
};

TEST_F(DeferredOsContextCreationTests, givenRegularEngineWhenCreatingOsContextThenOsContextIsInitializedDeferred) {
    DebugManagerStateRestore restore{};

    expectDeferredContextCreation(engineTypeUsageRegular, false);

    debugManager.flags.DeferOsContextInitialization.set(1);
    expectDeferredContextCreation(engineTypeUsageRegular, false);

    debugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageRegular, false);
}

TEST_F(DeferredOsContextCreationTests, givenDefaultEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageRegular, true);

    debugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageRegular, true);

    debugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageRegular, true);
}

TEST_F(DeferredOsContextCreationTests, givenInternalEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageInternal, false);

    debugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageInternal, false);

    debugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageInternal, false);
}

TEST_F(DeferredOsContextCreationTests, givenBlitterEngineWhenCreatingOsContextThenOsContextIsInitializedImmediately) {
    DebugManagerStateRestore restore{};

    expectImmediateContextCreation(engineTypeUsageBlitter, false);

    debugManager.flags.DeferOsContextInitialization.set(1);
    expectImmediateContextCreation(engineTypeUsageBlitter, false);

    debugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageBlitter, false);
}

TEST_F(DeferredOsContextCreationTests, givenEnsureContextInitializeCalledAndReturnsErrorThenOsContextIsNotInitialized) {
    struct MyOsContext : OsContext {
        MyOsContext(uint32_t contextId,
                    const EngineDescriptor &engineDescriptor) : OsContext(0, contextId, engineDescriptor) {}

        bool initializeContext(bool allocateInterrupt) override {
            initializeContextCalled++;
            return false;
        }

        size_t initializeContextCalled = 0u;
    };

    MyOsContext osContext{0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsageRegular)};
    EXPECT_FALSE(osContext.isInitialized());

    osContext.ensureContextInitialized(false);
    EXPECT_FALSE(osContext.isInitialized());
    EXPECT_EQ(1u, osContext.initializeContextCalled);
}

TEST_F(DeferredOsContextCreationTests, givenEnsureContextInitializeCalledMultipleTimesWhenOsContextIsCreatedThenInitializeOnlyOnce) {
    struct MyOsContext : OsContext {
        MyOsContext(uint32_t contextId,
                    const EngineDescriptor &engineDescriptor) : OsContext(0, contextId, engineDescriptor) {}

        bool initializeContext(bool allocateInterrupt) override {
            initializeContextCalled++;
            return true;
        }

        size_t initializeContextCalled = 0u;
    };

    MyOsContext osContext{0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsageRegular)};
    EXPECT_FALSE(osContext.isInitialized());

    osContext.ensureContextInitialized(false);
    EXPECT_TRUE(osContext.isInitialized());
    EXPECT_EQ(1u, osContext.initializeContextCalled);

    osContext.ensureContextInitialized(false);
    EXPECT_TRUE(osContext.isInitialized());
    EXPECT_EQ(1u, osContext.initializeContextCalled);
}

TEST_F(DeferredOsContextCreationTests, givenPrintOsContextInitializationsIsSetWhenOsContextItIsInitializedThenInfoIsLoggedToStdout) {
    DebugManagerStateRestore restore{};
    debugManager.flags.DeferOsContextInitialization.set(1);
    debugManager.flags.PrintOsContextInitializations.set(1);
    testing::internal::CaptureStdout();

    auto osContext = createOsContext(engineTypeUsageRegular, false);
    EXPECT_EQ(std::string{}, testing::internal::GetCapturedStdout());

    testing::internal::CaptureStdout();
    osContext->ensureContextInitialized(false);
    std::string expectedMessage = "OsContext initialization: contextId=0 usage=Regular type=RCS isRootDevice=0\n";
    EXPECT_EQ(expectedMessage, testing::internal::GetCapturedStdout());
}
