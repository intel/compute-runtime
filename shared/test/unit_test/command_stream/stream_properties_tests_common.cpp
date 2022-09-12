/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct MockFrontEndProperties : public FrontEndProperties {
    using FrontEndProperties::frontEndPropertiesSupport;
    using FrontEndProperties::propertiesSupportLoaded;
};

struct MockPipelineSelectProperties : public PipelineSelectProperties {
    using PipelineSelectProperties::pipelineSelectPropertiesSupport;
    using PipelineSelectProperties::propertiesSupportLoaded;
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
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    hwInfoConfig->fillFrontEndPropertiesSupportStructure(frontEndPropertiesSupport, *defaultHwInfo);

    for (auto isEngineInstanced : ::testing::Bool()) {
        for (auto isCooperativeKernel : ::testing::Bool()) {
            for (auto disableOverdispatch : ::testing::Bool()) {
                for (auto disableEUFusion : ::testing::Bool()) {
                    properties.frontEndState.setProperties(isCooperativeKernel, disableEUFusion, disableOverdispatch, isEngineInstanced, *defaultHwInfo);
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
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    hwInfoConfig->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    int32_t threadArbitrationPolicyValues[] = {
        ThreadArbitrationPolicy::AgeBased, ThreadArbitrationPolicy::RoundRobin,
        ThreadArbitrationPolicy::RoundRobinAfterDependency};

    PreemptionMode preemptionModes[] = {PreemptionMode::Disabled, PreemptionMode::Initial, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup, PreemptionMode::MidThread};

    StreamProperties properties;
    for (auto preemptionMode : preemptionModes) {
        for (auto requiresCoherency : ::testing::Bool()) {
            for (auto largeGrf : ::testing::Bool()) {
                for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
                    properties.stateComputeMode.setProperties(requiresCoherency, largeGrf ? 256 : 128, threadArbitrationPolicy, preemptionMode, *defaultHwInfo);
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
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, *defaultHwInfo);
        if (scmPropertiesSupport.zPassAsyncComputeThreadLimit) {
            EXPECT_EQ(forceZPassAsyncComputeThreadLimit, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        }
    }

    for (auto forcePixelAsyncComputeThreadLimit : ::testing::Bool()) {
        DebugManager.flags.ForcePixelAsyncComputeThreadLimit.set(forcePixelAsyncComputeThreadLimit);
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, *defaultHwInfo);
        if (scmPropertiesSupport.pixelAsyncComputeThreadLimit) {
            EXPECT_EQ(forcePixelAsyncComputeThreadLimit, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        }
    }

    for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
        DebugManager.flags.OverrideThreadArbitrationPolicy.set(threadArbitrationPolicy);
        properties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::MidBatch, *defaultHwInfo);
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

TEST(StreamPropertiesTests, givenSingleDispatchCcsFrontEndPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    FrontEndPropertiesSupport fePropertiesSupport{};
    auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    hwInfoConfig.fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);

    MockFrontEndProperties feProperties{};
    EXPECT_FALSE(feProperties.propertiesSupportLoaded);

    int32_t engineInstancedDevice = 1;

    feProperties.setPropertySingleSliceDispatchCcsMode(engineInstancedDevice, *defaultHwInfo);
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

    feProperties.setPropertySingleSliceDispatchCcsMode(engineInstancedDevice, *defaultHwInfo);
    EXPECT_TRUE(feProperties.singleSliceDispatchCcsMode.isDirty);
    EXPECT_EQ(engineInstancedDevice, feProperties.singleSliceDispatchCcsMode.value);
}

TEST(StreamPropertiesTests, whenSettingPipelineSelectPropertiesThenCorrectValueIsSet) {
    StreamProperties properties;

    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    hwInfoConfig->fillPipelineSelectPropertiesSupportStructure(pipelineSelectPropertiesSupport, *defaultHwInfo);

    for (auto modeSelected : ::testing::Bool()) {
        for (auto mediaSamplerDopClockGate : ::testing::Bool()) {
            for (auto systolicMode : ::testing::Bool()) {
                properties.pipelineSelect.setProperties(modeSelected, mediaSamplerDopClockGate, systolicMode, *defaultHwInfo);

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
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.modeSelected = false;
    pipeProperties.pipelineSelectPropertiesSupport.mediaSamplerDopClockGate = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;

    constexpr bool constState = false;
    bool changingState = false;
    pipeProperties.setProperties(changingState, constState, constState, *defaultHwInfo);

    // expect dirty as media and systolic changes from initial registered
    EXPECT_TRUE(pipeProperties.isDirty());

    changingState = !changingState;
    pipeProperties.setProperties(changingState, constState, constState, *defaultHwInfo);

    // expect clean as changed modeSelected is not supported
    EXPECT_FALSE(pipeProperties.isDirty());
}
