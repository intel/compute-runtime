/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct MockStateComputeModeProperties : public StateComputeModeProperties {
    using StateComputeModeProperties::propertiesSupportLoaded;
    using StateComputeModeProperties::scmPropertiesSupport;
};

struct MockFrontEndProperties : public FrontEndProperties {
    using FrontEndProperties::frontEndPropertiesSupport;
    using FrontEndProperties::propertiesSupportLoaded;
};

struct MockPipelineSelectProperties : public PipelineSelectProperties {
    using PipelineSelectProperties::pipelineSelectPropertiesSupport;
    using PipelineSelectProperties::propertiesSupportLoaded;
};

struct MockStateBaseAddressProperties : public StateBaseAddressProperties {
    using StateBaseAddressProperties::propertiesSupportLoaded;
    using StateBaseAddressProperties::stateBaseAddressPropertiesSupport;
};

TEST(StreamPropertiesTests, whenPropertyValueIsChangedThenProperStateIsSet) {
    NEO::StreamProperty streamProperty;

    EXPECT_EQ(-1, streamProperty.value);
    EXPECT_FALSE(streamProperty.isDirty);

    streamProperty.set(-1);
    EXPECT_EQ(-1, streamProperty.value);
    EXPECT_FALSE(streamProperty.isDirty);

    int32_t valuesToTest[] = {0, 1};
    for (auto valueToTest : valuesToTest) {
        streamProperty.set(valueToTest);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_TRUE(streamProperty.isDirty);

        streamProperty.isDirty = false;
        streamProperty.set(valueToTest);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_FALSE(streamProperty.isDirty);

        streamProperty.set(-1);
        EXPECT_EQ(valueToTest, streamProperty.value);
        EXPECT_FALSE(streamProperty.isDirty);
    }
}

TEST(StreamPropertiesTests, whenSettingCooperativeKernelPropertiesThenCorrectValueIsSet) {
    StreamProperties properties;

    FrontEndPropertiesSupport frontEndPropertiesSupport = {};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    productHelper.fillFrontEndPropertiesSupportStructure(frontEndPropertiesSupport, *defaultHwInfo);

    for (auto isEngineInstanced : ::testing::Bool()) {
        for (auto isCooperativeKernel : ::testing::Bool()) {
            for (auto disableOverdispatch : ::testing::Bool()) {
                for (auto disableEUFusion : ::testing::Bool()) {
                    properties.frontEndState.setProperties(isCooperativeKernel, disableEUFusion, disableOverdispatch, isEngineInstanced, rootDeviceEnvironment);
                    if (frontEndPropertiesSupport.computeDispatchAllWalker) {
                        EXPECT_EQ(isCooperativeKernel, properties.frontEndState.computeDispatchAllWalkerEnable.value);
                    } else {
                        EXPECT_EQ(-1, properties.frontEndState.computeDispatchAllWalkerEnable.value);
                    }
                    if (frontEndPropertiesSupport.disableEuFusion) {
                        EXPECT_EQ(disableEUFusion, properties.frontEndState.disableEUFusion.value);
                    } else {
                        EXPECT_EQ(-1, properties.frontEndState.disableEUFusion.value);
                    }
                    if (frontEndPropertiesSupport.disableOverdispatch) {
                        EXPECT_EQ(disableOverdispatch, properties.frontEndState.disableOverdispatch.value);
                    } else {
                        EXPECT_EQ(-1, properties.frontEndState.disableOverdispatch.value);
                    }
                    if (frontEndPropertiesSupport.singleSliceDispatchCcsMode) {
                        EXPECT_EQ(isEngineInstanced, properties.frontEndState.singleSliceDispatchCcsMode.value);
                    } else {
                        EXPECT_EQ(-1, properties.frontEndState.singleSliceDispatchCcsMode.value);
                    }
                }
            }
        }
    }
}

TEST(StreamPropertiesTests, whenSettingStateComputeModePropertiesThenCorrectValuesAreSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);

    int32_t threadArbitrationPolicyValues[] = {
        ThreadArbitrationPolicy::AgeBased, ThreadArbitrationPolicy::RoundRobin,
        ThreadArbitrationPolicy::RoundRobinAfterDependency};

    PreemptionMode preemptionModes[] = {PreemptionMode::Disabled, PreemptionMode::Initial, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup, PreemptionMode::MidThread};

    StreamProperties properties;
    for (auto preemptionMode : preemptionModes) {
        for (auto requiresCoherency : ::testing::Bool()) {
            for (auto largeGrf : ::testing::Bool()) {
                for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
                    properties.stateComputeMode.setProperties(requiresCoherency, largeGrf ? 256 : 128, threadArbitrationPolicy, preemptionMode, rootDeviceEnvironment);
                    EXPECT_EQ(largeGrf, properties.stateComputeMode.largeGrfMode.value);
                    if (scmPropertiesSupport.coherencyRequired) {
                        EXPECT_EQ(requiresCoherency, properties.stateComputeMode.isCoherencyRequired.value);
                    } else {
                        EXPECT_EQ(-1, properties.stateComputeMode.isCoherencyRequired.value);
                    }
                    EXPECT_EQ(-1, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
                    EXPECT_EQ(-1, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
                    EXPECT_EQ(threadArbitrationPolicy, properties.stateComputeMode.threadArbitrationPolicy.value);

                    if (scmPropertiesSupport.devicePreemptionMode) {
                        EXPECT_EQ(preemptionMode, static_cast<PreemptionMode>(properties.stateComputeMode.devicePreemptionMode.value));
                    } else {
                        EXPECT_EQ(-1, properties.stateComputeMode.devicePreemptionMode.value);
                    }
                }
            }
        }
    }

    for (auto forceZPassAsyncComputeThreadLimit : ::testing::Bool()) {
        DebugManager.flags.ForceZPassAsyncComputeThreadLimit.set(forceZPassAsyncComputeThreadLimit);
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, rootDeviceEnvironment);
        if (scmPropertiesSupport.zPassAsyncComputeThreadLimit) {
            EXPECT_EQ(forceZPassAsyncComputeThreadLimit, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        }
    }

    for (auto forcePixelAsyncComputeThreadLimit : ::testing::Bool()) {
        DebugManager.flags.ForcePixelAsyncComputeThreadLimit.set(forcePixelAsyncComputeThreadLimit);
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, rootDeviceEnvironment);
        if (scmPropertiesSupport.pixelAsyncComputeThreadLimit) {
            EXPECT_EQ(forcePixelAsyncComputeThreadLimit, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        }
    }

    for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
        DebugManager.flags.OverrideThreadArbitrationPolicy.set(threadArbitrationPolicy);
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, rootDeviceEnvironment);
        if (scmPropertiesSupport.threadArbitrationPolicy) {
            EXPECT_EQ(threadArbitrationPolicy, properties.stateComputeMode.threadArbitrationPolicy.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.threadArbitrationPolicy.value);
        }
    }
}

template <typename PropertiesT>
using getAllPropertiesFunctionPtr = std::vector<StreamProperty *> (*)(PropertiesT &properties);

template <typename PropertiesT, getAllPropertiesFunctionPtr<PropertiesT> getAllProperties>
void verifyIsDirty() {
    struct MockPropertiesT : PropertiesT {
        using PropertiesT::clearIsDirty;
    };
    MockPropertiesT properties;
    auto allProperties = getAllProperties(properties);

    EXPECT_FALSE(properties.isDirty());
    for (auto pProperty : allProperties) {
        pProperty->isDirty = true;
        EXPECT_TRUE(properties.isDirty());
        pProperty->isDirty = false;
        EXPECT_FALSE(properties.isDirty());
    }
    for (auto pProperty : allProperties) {
        pProperty->isDirty = true;
    }

    EXPECT_EQ(!allProperties.empty(), properties.isDirty());

    properties.clearIsDirty();
    for (auto pProperty : allProperties) {
        EXPECT_FALSE(pProperty->isDirty);
    }
    EXPECT_FALSE(properties.isDirty());
}

TEST(StreamPropertiesTests, givenVariousStatesOfStateComputeModePropertiesWhenIsDirtyIsQueriedThenCorrectValueIsReturned) {
    verifyIsDirty<StateComputeModeProperties, getAllStateComputeModeProperties>();
}

TEST(StreamPropertiesTests, givenVariousStatesOfFrontEndPropertiesWhenIsDirtyIsQueriedThenCorrectValueIsReturned) {
    verifyIsDirty<FrontEndProperties, getAllFrontEndProperties>();
}

TEST(StreamPropertiesTests, givenVariousStatesOfPipelineSelectPropertiesWhenIsDirtyIsQueriedThenCorrectValueIsReturned) {
    verifyIsDirty<PipelineSelectProperties, getAllPipelineSelectProperties>();
}

template <typename PropertiesT, getAllPropertiesFunctionPtr<PropertiesT> getAllProperties>
void verifySettingPropertiesFromOtherStruct() {
    PropertiesT propertiesDestination;
    PropertiesT propertiesSource;

    auto allPropertiesDestination = getAllProperties(propertiesDestination);
    auto allPropertiesSource = getAllProperties(propertiesSource);

    int valueToSet = 1;
    for (auto pProperty : allPropertiesSource) {
        pProperty->value = valueToSet;
        valueToSet++;
    }
    EXPECT_FALSE(propertiesSource.isDirty());

    propertiesDestination.setProperties(propertiesSource);
    EXPECT_EQ(!allPropertiesDestination.empty(), propertiesDestination.isDirty());

    int expectedValue = 1;
    for (auto pProperty : allPropertiesDestination) {
        EXPECT_EQ(expectedValue, pProperty->value);
        EXPECT_TRUE(pProperty->isDirty);
        expectedValue++;
    }

    propertiesDestination.setProperties(propertiesSource);
    EXPECT_FALSE(propertiesDestination.isDirty());

    expectedValue = 1;
    for (auto pProperty : allPropertiesDestination) {
        EXPECT_EQ(expectedValue, pProperty->value);
        EXPECT_FALSE(pProperty->isDirty);
        expectedValue++;
    }
}

TEST(StreamPropertiesTests, givenOtherStateComputeModePropertiesStructWhenSetPropertiesIsCalledThenCorrectValuesAreSet) {
    verifySettingPropertiesFromOtherStruct<StateComputeModeProperties, &getAllStateComputeModeProperties>();
}

TEST(StreamPropertiesTests, givenOtherFrontEndPropertiesStructWhenSetPropertiesIsCalledThenCorrectValuesAreSet) {
    verifySettingPropertiesFromOtherStruct<FrontEndProperties, getAllFrontEndProperties>();
}

TEST(StreamPropertiesTests, givenOtherPipelineSelectPropertiesStructWhenSetPropertiesIsCalledThenCorrectValuesAreSet) {
    verifySettingPropertiesFromOtherStruct<PipelineSelectProperties, getAllPipelineSelectProperties>();
}

TEST(StreamPropertiesTests, givenCoherencyStateAndDevicePreemptionComputeModePropertiesWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.coherencyRequired = false;
    scmProperties.scmPropertiesSupport.devicePreemptionMode = false;

    bool coherencyRequired = false;
    PreemptionMode devicePreemptionMode = PreemptionMode::Disabled;
    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(-1, scmProperties.devicePreemptionMode.value);

    scmProperties.scmPropertiesSupport.coherencyRequired = true;
    scmProperties.scmPropertiesSupport.devicePreemptionMode = true;
    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    devicePreemptionMode = PreemptionMode::Initial;
    scmProperties.setProperties(coherencyRequired, -1, -1, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    coherencyRequired = true;
    devicePreemptionMode = PreemptionMode::MidThread;
    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    scmProperties.setPropertiesCoherencyDevicePreemption(coherencyRequired, devicePreemptionMode, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);
}

TEST(StreamPropertiesTests, givenGrfNumberAndThreadArbitrationStateComputeModePropertiesWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.largeGrfMode = false;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = false;

    int32_t grfNumber = 128;
    int32_t threadArbitration = 1;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(-1, scmProperties.threadArbitrationPolicy.value);

    scmProperties.scmPropertiesSupport.largeGrfMode = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;
    scmProperties.setProperties(false, static_cast<uint32_t>(grfNumber), threadArbitration, PreemptionMode::Initial, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    grfNumber = 256;
    threadArbitration = 2;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);
}

TEST(StreamPropertiesTests, givenForceDebugDefaultThreadArbitrationStateComputeModePropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    DebugManagerStateRestore restorer;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto defaultThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();

    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;

    constexpr int32_t grfNumber = -1;
    constexpr int32_t requestedThreadArbitration = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    int32_t threadArbitration = requestedThreadArbitration;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(requestedThreadArbitration, scmProperties.threadArbitrationPolicy.value);

    DebugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.set(true);
    threadArbitration = ThreadArbitrationPolicy::NotPresent;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration, rootDeviceEnvironment);
    if (defaultThreadArbitrationPolicy == requestedThreadArbitration) {
        EXPECT_FALSE(scmProperties.isDirty());
    } else {
        EXPECT_TRUE(scmProperties.isDirty());
    }
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(defaultThreadArbitrationPolicy, scmProperties.threadArbitrationPolicy.value);
}

TEST(StreamPropertiesTests, givenSingleDispatchCcsFrontEndPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);

    MockFrontEndProperties feProperties{};
    EXPECT_FALSE(feProperties.propertiesSupportLoaded);

    int32_t engineInstancedDevice = 1;

    feProperties.setPropertySingleSliceDispatchCcsMode(engineInstancedDevice, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(feProperties.propertiesSupportLoaded);
    if (fePropertiesSupport.singleSliceDispatchCcsMode) {
        EXPECT_TRUE(feProperties.singleSliceDispatchCcsMode.isDirty);
        EXPECT_EQ(engineInstancedDevice, feProperties.singleSliceDispatchCcsMode.value);
    } else {
        EXPECT_FALSE(feProperties.singleSliceDispatchCcsMode.isDirty);
        EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);
    }

    feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode = true;
    engineInstancedDevice = 2;

    feProperties.setPropertySingleSliceDispatchCcsMode(engineInstancedDevice, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(feProperties.singleSliceDispatchCcsMode.isDirty);
    EXPECT_EQ(engineInstancedDevice, feProperties.singleSliceDispatchCcsMode.value);
}

TEST(StreamPropertiesTests, givenDisableOverdispatchFrontEndPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.disableOverdispatch = false;

    bool disableOverdispatch = false;
    feProperties.setPropertyDisableOverdispatch(disableOverdispatch, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(-1, feProperties.disableOverdispatch.value);

    feProperties.frontEndPropertiesSupport.disableOverdispatch = true;
    feProperties.setProperties(false, false, disableOverdispatch, -1, rootDeviceEnvironment);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);

    feProperties.setPropertyDisableOverdispatch(disableOverdispatch, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);

    feProperties.setPropertyDisableOverdispatch(disableOverdispatch, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);

    disableOverdispatch = true;
    feProperties.setPropertyDisableOverdispatch(disableOverdispatch, rootDeviceEnvironment);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableOverdispatch.value);

    feProperties.setPropertyDisableOverdispatch(disableOverdispatch, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableOverdispatch.value);
}

TEST(StreamPropertiesTests, givenComputeDispatchAllWalkerEnableAndDisableEuFusionFrontEndPropertiesWhenSettingPropertiesAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.disableEuFusion = false;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = false;

    bool disableEuFusion = false;
    bool isCooperativeKernel = false;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(-1, feProperties.disableEUFusion.value);
    EXPECT_EQ(-1, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.frontEndPropertiesSupport.disableEuFusion = true;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = true;
    feProperties.setProperties(isCooperativeKernel, disableEuFusion, false, -1, rootDeviceEnvironment);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    disableEuFusion = true;
    isCooperativeKernel = true;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion, rootDeviceEnvironment);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableEUFusion.value);
    EXPECT_EQ(1, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion, rootDeviceEnvironment);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableEUFusion.value);
    EXPECT_EQ(1, feProperties.computeDispatchAllWalkerEnable.value);
}

TEST(StreamPropertiesTests, whenSettingPipelineSelectPropertiesThenCorrectValueIsSet) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    StreamProperties properties;

    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    productHelper.fillPipelineSelectPropertiesSupportStructure(pipelineSelectPropertiesSupport, *defaultHwInfo);

    for (auto modeSelected : ::testing::Bool()) {
        for (auto mediaSamplerDopClockGate : ::testing::Bool()) {
            for (auto systolicMode : ::testing::Bool()) {
                properties.pipelineSelect.setProperties(modeSelected, mediaSamplerDopClockGate, systolicMode, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

                if (pipelineSelectPropertiesSupport.modeSelected) {
                    EXPECT_EQ(modeSelected, properties.pipelineSelect.modeSelected.value);
                } else {
                    EXPECT_EQ(-1, properties.pipelineSelect.modeSelected.value);
                }
                if (pipelineSelectPropertiesSupport.mediaSamplerDopClockGate) {
                    EXPECT_EQ(mediaSamplerDopClockGate, properties.pipelineSelect.mediaSamplerDopClockGate.value);
                } else {
                    EXPECT_EQ(-1, properties.pipelineSelect.mediaSamplerDopClockGate.value);
                }
                if (pipelineSelectPropertiesSupport.systolicMode) {
                    EXPECT_EQ(systolicMode, properties.pipelineSelect.systolicMode.value);
                } else {
                    EXPECT_EQ(-1, properties.pipelineSelect.systolicMode.value);
                }
            }
        }
    }
}

TEST(StreamPropertiesTests, givenModeSelectPipelineSelectPropertyNotSupportedWhenSettingPropertyAndCheckIfDirtyThenExpectCleanState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.modeSelected = false;
    pipeProperties.pipelineSelectPropertiesSupport.mediaSamplerDopClockGate = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;

    constexpr bool constState = false;
    bool changingState = false;
    pipeProperties.setProperties(changingState, constState, constState, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    // expect dirty as media and systolic changes from initial registered
    EXPECT_TRUE(pipeProperties.isDirty());

    changingState = !changingState;
    pipeProperties.setProperties(changingState, constState, constState, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    // expect clean as changed modeSelected is not supported
    EXPECT_FALSE(pipeProperties.isDirty());
}

TEST(StreamPropertiesTests, givenSystolicModePipelineSelectPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = false;

    bool systolicMode = false;
    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(-1, pipeProperties.systolicMode.value);

    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;
    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.systolicMode.value);

    systolicMode = true;
    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode, rootDeviceEnvironment);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);
}

TEST(StreamPropertiesTests, givenStateBaseAddressSupportFlagStateWhenSettingPropertyAndCheckIfDirtyThenExpectCleanStateForNotSupportedAndDirtyForSupported) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.globalAtomics = false;
    sbaProperties.stateBaseAddressPropertiesSupport.statelessMocs = false;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = false;

    sbaProperties.setProperties(true, 1, 1, 1, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_FALSE(sbaProperties.isDirty());

    EXPECT_EQ(-1, sbaProperties.globalAtomics.value);
    EXPECT_EQ(-1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.globalAtomics = true;
    sbaProperties.setProperties(true, 1, 0, 0, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_TRUE(sbaProperties.globalAtomics.isDirty);
    EXPECT_FALSE(sbaProperties.statelessMocs.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolSize.isDirty);

    EXPECT_EQ(1, sbaProperties.globalAtomics.value);
    EXPECT_EQ(-1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.globalAtomics = false;
    sbaProperties.stateBaseAddressPropertiesSupport.statelessMocs = true;
    sbaProperties.setProperties(false, 1, 1, 1, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_FALSE(sbaProperties.globalAtomics.isDirty);
    EXPECT_TRUE(sbaProperties.statelessMocs.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolSize.isDirty);

    EXPECT_EQ(1, sbaProperties.globalAtomics.value);
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.statelessMocs = false;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = true;
    sbaProperties.setProperties(true, 2, 2, 2, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_FALSE(sbaProperties.globalAtomics.isDirty);
    EXPECT_FALSE(sbaProperties.statelessMocs.isDirty);
    EXPECT_TRUE(sbaProperties.bindingTablePoolBaseAddress.isDirty);
    EXPECT_TRUE(sbaProperties.bindingTablePoolSize.isDirty);

    EXPECT_EQ(1, sbaProperties.globalAtomics.value);
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(2, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.globalAtomics = true;
    sbaProperties.stateBaseAddressPropertiesSupport.statelessMocs = true;
    sbaProperties.setProperties(true, 1, 2, 2, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_FALSE(sbaProperties.isDirty());

    sbaProperties.setProperties(false, 0, 3, 2, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());

    EXPECT_EQ(0, sbaProperties.globalAtomics.value);
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);
    EXPECT_EQ(3, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);

    sbaProperties.setProperties(false, 0, 3, 3, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());

    EXPECT_EQ(0, sbaProperties.globalAtomics.value);
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);
    EXPECT_EQ(3, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(3u, sbaProperties.bindingTablePoolSize.value);

    MockStateBaseAddressProperties copySbaProperties{};

    copySbaProperties.setProperties(sbaProperties);
    EXPECT_TRUE(copySbaProperties.isDirty());

    EXPECT_EQ(0, copySbaProperties.globalAtomics.value);
    EXPECT_EQ(0, copySbaProperties.statelessMocs.value);
    EXPECT_EQ(3, copySbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(3u, copySbaProperties.bindingTablePoolSize.value);

    sbaProperties.setProperties(copySbaProperties);
    EXPECT_FALSE(sbaProperties.isDirty());
}

TEST(StreamPropertiesTests, givenStateBaseAddressSupportFlagDefaultValueWhenSettingPropertyAndCheckIfDirtyThenExpectValueSetForSupportedAndCleanForNotSupported) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    StateBaseAddressProperties sbaProperties{};

    sbaProperties.setProperties(true, 2, 3, 3, -1, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    if (sbaPropertiesSupport.globalAtomics) {
        EXPECT_EQ(1, sbaProperties.globalAtomics.value);
    } else {
        EXPECT_EQ(-1, sbaProperties.globalAtomics.value);
    }

    if (sbaPropertiesSupport.statelessMocs) {
        EXPECT_EQ(2, sbaProperties.statelessMocs.value);
    } else {
        EXPECT_EQ(-1, sbaProperties.statelessMocs.value);
    }

    if (sbaPropertiesSupport.bindingTablePoolBaseAddress) {
        EXPECT_EQ(3, sbaProperties.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(3u, sbaProperties.bindingTablePoolSize.value);
    } else {
        EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
        EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);
    }
}

TEST(StreamPropertiesTests, givenStateBaseAddressCommonBaseAddressAndSizeWhenSettingAddressSizePropertiesThenExpectCorrectDirtyFlagAndStateValue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.globalAtomics = false;
    sbaProperties.stateBaseAddressPropertiesSupport.statelessMocs = false;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = false;

    sbaProperties.setProperties(false, -1, -1, 10, 10, -1, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(10, sbaProperties.surfaceStateBaseAddress.value);

    EXPECT_TRUE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, -1, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(20u, sbaProperties.surfaceStateSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_TRUE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, 30, -1, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(30, sbaProperties.dynamicStateBaseAddress.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_TRUE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, 30, 40, -1, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(40u, sbaProperties.dynamicStateSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_TRUE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, 30, 40, 50, -1, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(50, sbaProperties.indirectObjectBaseAddress.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_TRUE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, 30, 40, 50, 60, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(60u, sbaProperties.indirectObjectSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_TRUE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setProperties(false, -1, -1, 10, 10, 20, 30, 40, 50, 60, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_FALSE(sbaProperties.isDirty());
}
