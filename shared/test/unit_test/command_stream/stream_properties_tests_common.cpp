/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "test_traits_common.h"

using namespace NEO;

struct MockStateComputeModeProperties : public StateComputeModeProperties {
    using StateComputeModeProperties::defaultThreadArbitrationPolicy;
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

using StreamPropertiesTests = ::testing::Test;

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
    properties.initSupport(rootDeviceEnvironment);

    for (auto isCooperativeKernel : ::testing::Bool()) {
        for (auto disableOverdispatch : ::testing::Bool()) {
            for (auto disableEUFusion : ::testing::Bool()) {
                properties.frontEndState.setPropertiesAll(isCooperativeKernel, disableEUFusion, disableOverdispatch);
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
                    EXPECT_EQ(-1, properties.frontEndState.singleSliceDispatchCcsMode.value);
                } else {
                    EXPECT_EQ(-1, properties.frontEndState.singleSliceDispatchCcsMode.value);
                }
            }
        }
    }
}

HWTEST2_F(StreamPropertiesTests, whenSettingStateComputeModePropertiesThenCorrectValuesAreSet, IsAtMostXe3Core) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    productHelper.fillScmPropertiesSupportStructure(scmPropertiesSupport);
    productHelper.fillScmPropertiesSupportStructureExtra(scmPropertiesSupport, rootDeviceEnvironment);

    int32_t threadArbitrationPolicyValues[] = {
        ThreadArbitrationPolicy::AgeBased, ThreadArbitrationPolicy::RoundRobin,
        ThreadArbitrationPolicy::RoundRobinAfterDependency};

    PreemptionMode preemptionModes[] = {PreemptionMode::Disabled, PreemptionMode::Initial, PreemptionMode::MidBatch, PreemptionMode::ThreadGroup, PreemptionMode::MidThread};

    StreamProperties properties;
    properties.initSupport(rootDeviceEnvironment);

    for (auto preemptionMode : preemptionModes) {
        for (auto requiresCoherency : ::testing::Bool()) {
            for (auto largeGrf : ::testing::Bool()) {
                for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
                    properties.stateComputeMode.setPropertiesAll(requiresCoherency, largeGrf ? 256 : 128, threadArbitrationPolicy, preemptionMode, false);
                    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
                        EXPECT_EQ(largeGrf, properties.stateComputeMode.largeGrfMode.value);
                    } else {
                        EXPECT_EQ(-1, properties.stateComputeMode.largeGrfMode.value);
                    }
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
        debugManager.flags.ForceZPassAsyncComputeThreadLimit.set(forceZPassAsyncComputeThreadLimit);
        properties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::MidBatch, false);
        if (scmPropertiesSupport.zPassAsyncComputeThreadLimit) {
            EXPECT_EQ(forceZPassAsyncComputeThreadLimit, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.zPassAsyncComputeThreadLimit.value);
        }
    }

    for (auto forcePixelAsyncComputeThreadLimit : ::testing::Bool()) {
        debugManager.flags.ForcePixelAsyncComputeThreadLimit.set(forcePixelAsyncComputeThreadLimit);
        properties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::MidBatch, false);
        if (scmPropertiesSupport.pixelAsyncComputeThreadLimit) {
            EXPECT_EQ(forcePixelAsyncComputeThreadLimit, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.pixelAsyncComputeThreadLimit.value);
        }
    }

    for (auto threadArbitrationPolicy : threadArbitrationPolicyValues) {
        debugManager.flags.OverrideThreadArbitrationPolicy.set(threadArbitrationPolicy);
        properties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::MidBatch, false);
        if (scmPropertiesSupport.threadArbitrationPolicy) {
            EXPECT_EQ(threadArbitrationPolicy, properties.stateComputeMode.threadArbitrationPolicy.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.threadArbitrationPolicy.value);
        }
    }

    for (auto forceScratchAndMTPBufferSizeMode : ::testing::Bool()) {
        debugManager.flags.ForceScratchAndMTPBufferSizeMode.set(forceScratchAndMTPBufferSizeMode);
        properties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::MidBatch, false);
        if (scmPropertiesSupport.allocationForScratchAndMidthreadPreemption) {
            EXPECT_EQ(forceScratchAndMTPBufferSizeMode, properties.stateComputeMode.memoryAllocationForScratchAndMidthreadPreemptionBuffers.value);
        } else {
            EXPECT_EQ(-1, properties.stateComputeMode.memoryAllocationForScratchAndMidthreadPreemptionBuffers.value);
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

    propertiesDestination.copyPropertiesAll(propertiesSource);
    EXPECT_EQ(!allPropertiesDestination.empty(), propertiesDestination.isDirty());

    int expectedValue = 1;
    for (auto pProperty : allPropertiesDestination) {
        EXPECT_EQ(expectedValue, pProperty->value);
        EXPECT_TRUE(pProperty->isDirty);
        expectedValue++;
    }

    propertiesDestination.copyPropertiesAll(propertiesSource);
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

TEST(StreamPropertiesTests, givenVariousDevicePreemptionComputeModesWhenSettingPropertyPerContextAndCheckIfSupportedThenExpectCorrectState) {
    bool clearDirtyState = false;
    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.devicePreemptionMode = false;

    bool coherencyRequired = false;
    PreemptionMode devicePreemptionMode = PreemptionMode::Disabled;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.devicePreemptionMode.value);

    scmProperties.scmPropertiesSupport.devicePreemptionMode = true;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    devicePreemptionMode = PreemptionMode::Initial;
    scmProperties.setPropertiesAll(coherencyRequired, -1, -1, devicePreemptionMode, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    devicePreemptionMode = PreemptionMode::MidThread;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);

    clearDirtyState = true;
    devicePreemptionMode = PreemptionMode::ThreadGroup;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);
}

TEST(StreamPropertiesTests, givenVariousCoherencyRequirementsWhenSettingPropertyPerContextAndCheckIfSupportedThenExpectCorrectState) {
    bool clearDirtyState = false;
    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.coherencyRequired = false;

    bool coherencyRequired = false;
    PreemptionMode devicePreemptionMode = PreemptionMode::Disabled;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.isCoherencyRequired.value);

    scmProperties.scmPropertiesSupport.coherencyRequired = true;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);

    scmProperties.setPropertiesAll(coherencyRequired, -1, -1, devicePreemptionMode, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);

    coherencyRequired = true;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.isCoherencyRequired.value);

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.isCoherencyRequired.value);

    clearDirtyState = true;
    coherencyRequired = false;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
}

TEST(StreamPropertiesTests, givenVariableRegisterSizeAllocationSettingWhenSettingPropertyPerContextAndCheckIfSupportedThenSetDirtyOnlyOnce) {
    bool clearDirtyState = false;
    bool coherencyRequired = false;
    PreemptionMode devicePreemptionMode = PreemptionMode::Disabled;

    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.enableVariableRegisterSizeAllocation = false;

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.enableVariableRegisterSizeAllocation.value);

    scmProperties.scmPropertiesSupport.enableVariableRegisterSizeAllocation = true;
    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.enableVariableRegisterSizeAllocation.value);

    scmProperties.setPropertiesAll(coherencyRequired, -1, -1, devicePreemptionMode, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.enableVariableRegisterSizeAllocation.value);

    scmProperties.setPropertiesPerContext(coherencyRequired, devicePreemptionMode, clearDirtyState, false);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.enableVariableRegisterSizeAllocation.value);
}

TEST(StreamPropertiesTests, givenGrfNumberAndThreadArbitrationStateComputeModePropertiesWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.largeGrfMode = false;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = false;

    int32_t grfNumber = 128;
    int32_t threadArbitration = 1;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(-1, scmProperties.threadArbitrationPolicy.value);

    scmProperties.scmPropertiesSupport.largeGrfMode = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;
    scmProperties.setPropertiesAll(false, static_cast<uint32_t>(grfNumber), threadArbitration, PreemptionMode::Initial, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    grfNumber = 256;
    threadArbitration = 2;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);
}

TEST(StreamPropertiesTests, givenSetAllStateComputeModePropertiesWhenResettingStateThenResetValuesAndDirtyKeepSupportFlagLoaded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceScratchAndMTPBufferSizeMode.set(2);
    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.coherencyRequired = true;
    scmProperties.scmPropertiesSupport.largeGrfMode = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;
    scmProperties.scmPropertiesSupport.devicePreemptionMode = true;
    scmProperties.scmPropertiesSupport.allocationForScratchAndMidthreadPreemption = true;
    scmProperties.scmPropertiesSupport.enableVariableRegisterSizeAllocation = true;

    int32_t grfNumber = 128;
    int32_t threadArbitration = 1;
    PreemptionMode devicePreemptionMode = PreemptionMode::Initial;
    bool coherency = false;
    scmProperties.setPropertiesAll(coherency, static_cast<uint32_t>(grfNumber), threadArbitration, devicePreemptionMode, false);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);
    EXPECT_EQ(0, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(static_cast<int32_t>(devicePreemptionMode), scmProperties.devicePreemptionMode.value);
    EXPECT_EQ(2, scmProperties.memoryAllocationForScratchAndMidthreadPreemptionBuffers.value);
    EXPECT_EQ(1, scmProperties.enableVariableRegisterSizeAllocation.value);

    scmProperties.resetState();
    EXPECT_FALSE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(-1, scmProperties.threadArbitrationPolicy.value);
    EXPECT_EQ(-1, scmProperties.isCoherencyRequired.value);
    EXPECT_EQ(-1, scmProperties.devicePreemptionMode.value);
    EXPECT_EQ(-1, scmProperties.memoryAllocationForScratchAndMidthreadPreemptionBuffers.value);
    EXPECT_EQ(-1, scmProperties.enableVariableRegisterSizeAllocation.value);

    EXPECT_TRUE(scmProperties.propertiesSupportLoaded);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.coherencyRequired);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.largeGrfMode);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.threadArbitrationPolicy);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.devicePreemptionMode);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.allocationForScratchAndMidthreadPreemption);
    EXPECT_TRUE(scmProperties.scmPropertiesSupport.enableVariableRegisterSizeAllocation);
}

TEST(StreamPropertiesTests, givenGrfNumberAndThreadArbitrationStateComputeModePropertiesWhenCopyingPropertyAndCheckIfDirtyThenExpectCorrectState) {
    MockStateComputeModeProperties scmProperties{};
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.largeGrfMode = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;

    int32_t grfNumber = 128;
    int32_t threadArbitration = 1;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(0, scmProperties.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmProperties.threadArbitrationPolicy.value);

    MockStateComputeModeProperties scmPropertiesCopy{};

    scmPropertiesCopy.copyPropertiesGrfNumberThreadArbitration(scmProperties);
    EXPECT_TRUE(scmPropertiesCopy.isDirty());
    EXPECT_EQ(0, scmPropertiesCopy.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmPropertiesCopy.threadArbitrationPolicy.value);

    scmPropertiesCopy.copyPropertiesGrfNumberThreadArbitration(scmProperties);
    EXPECT_FALSE(scmPropertiesCopy.isDirty());
    EXPECT_EQ(0, scmPropertiesCopy.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmPropertiesCopy.threadArbitrationPolicy.value);

    grfNumber = 256;
    threadArbitration = 2;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);

    scmPropertiesCopy.copyPropertiesGrfNumberThreadArbitration(scmProperties);
    EXPECT_TRUE(scmPropertiesCopy.isDirty());
    EXPECT_EQ(1, scmPropertiesCopy.largeGrfMode.value);
    EXPECT_EQ(threadArbitration, scmPropertiesCopy.threadArbitrationPolicy.value);
}

TEST(StreamPropertiesTests, givenForceDebugDefaultThreadArbitrationStateComputeModePropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    DebugManagerStateRestore restorer;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto defaultThreadArbitrationPolicy = gfxCoreHelper.getDefaultThreadArbitrationPolicy();

    MockStateComputeModeProperties scmProperties{};
    scmProperties.defaultThreadArbitrationPolicy = defaultThreadArbitrationPolicy;
    scmProperties.propertiesSupportLoaded = true;
    scmProperties.scmPropertiesSupport.threadArbitrationPolicy = true;

    constexpr int32_t grfNumber = -1;
    constexpr int32_t requestedThreadArbitration = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    int32_t threadArbitration = requestedThreadArbitration;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
    EXPECT_TRUE(scmProperties.isDirty());
    EXPECT_EQ(-1, scmProperties.largeGrfMode.value);
    EXPECT_EQ(requestedThreadArbitration, scmProperties.threadArbitrationPolicy.value);

    debugManager.flags.ForceDefaultThreadArbitrationPolicyIfNotSpecified.set(true);
    threadArbitration = ThreadArbitrationPolicy::NotPresent;
    scmProperties.setPropertiesGrfNumberThreadArbitration(static_cast<uint32_t>(grfNumber), threadArbitration);
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
    feProperties.initSupport(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(feProperties.propertiesSupportLoaded);

    if (fePropertiesSupport.singleSliceDispatchCcsMode) {
        EXPECT_FALSE(feProperties.singleSliceDispatchCcsMode.isDirty);
    } else {
        EXPECT_FALSE(feProperties.singleSliceDispatchCcsMode.isDirty);
        EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);
    }

    feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode = true;
    EXPECT_FALSE(feProperties.singleSliceDispatchCcsMode.isDirty);
}

TEST(StreamPropertiesTests, givenDisableOverdispatchFrontEndPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    bool clearDirtyState = false;
    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.disableOverdispatch = false;
    feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode = false;

    bool disableOverdispatch = false;
    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(-1, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.frontEndPropertiesSupport.disableOverdispatch = true;
    feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode = true;
    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.setPropertiesAll(false, false, disableOverdispatch);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    disableOverdispatch = true;
    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    clearDirtyState = true;
    disableOverdispatch = false;

    feProperties.setPropertiesDisableOverdispatch(disableOverdispatch, clearDirtyState);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);
}

TEST(StreamPropertiesTests, givenComputeDispatchAllWalkerEnableAndDisableEuFusionFrontEndPropertiesWhenSettingPropertiesAndCheckIfSupportedThenExpectCorrectState) {
    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.disableEuFusion = false;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = false;

    bool disableEuFusion = false;
    bool isCooperativeKernel = false;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(-1, feProperties.disableEUFusion.value);
    EXPECT_EQ(-1, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.frontEndPropertiesSupport.disableEuFusion = true;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = true;
    feProperties.setPropertiesAll(isCooperativeKernel, disableEuFusion, false);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    disableEuFusion = true;
    isCooperativeKernel = true;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableEUFusion.value);
    EXPECT_EQ(1, feProperties.computeDispatchAllWalkerEnable.value);

    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(1, feProperties.disableEUFusion.value);
    EXPECT_EQ(1, feProperties.computeDispatchAllWalkerEnable.value);
}

TEST(StreamPropertiesTests, givenComputeDispatchAllWalkerEnableAndDisableEuFusionFrontEndPropertiesWhenCopyingPropertiesAndCheckIfDirtyThenExpectCorrectState) {
    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.disableEuFusion = true;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = true;

    bool disableEuFusion = false;
    bool isCooperativeKernel = false;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.disableEUFusion.value);
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);

    MockFrontEndProperties fePropertiesCopy{};

    fePropertiesCopy.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(feProperties);
    EXPECT_TRUE(fePropertiesCopy.isDirty());
    EXPECT_EQ(0, fePropertiesCopy.disableEUFusion.value);
    EXPECT_EQ(0, fePropertiesCopy.computeDispatchAllWalkerEnable.value);

    fePropertiesCopy.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(feProperties);
    EXPECT_FALSE(fePropertiesCopy.isDirty());
    EXPECT_EQ(0, fePropertiesCopy.disableEUFusion.value);
    EXPECT_EQ(0, fePropertiesCopy.computeDispatchAllWalkerEnable.value);

    disableEuFusion = true;
    isCooperativeKernel = true;
    feProperties.setPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(isCooperativeKernel, disableEuFusion);

    fePropertiesCopy.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(feProperties);
    EXPECT_TRUE(fePropertiesCopy.isDirty());
    EXPECT_EQ(1, fePropertiesCopy.disableEUFusion.value);
    EXPECT_EQ(1, fePropertiesCopy.computeDispatchAllWalkerEnable.value);
}

TEST(StreamPropertiesTests, givenSetAllFrontEndPropertiesWhenResettingStateThenResetValuesAndDirtyKeepSupportFlagLoaded) {
    MockFrontEndProperties feProperties{};
    feProperties.propertiesSupportLoaded = true;
    feProperties.frontEndPropertiesSupport.computeDispatchAllWalker = true;
    feProperties.frontEndPropertiesSupport.disableEuFusion = true;
    feProperties.frontEndPropertiesSupport.disableOverdispatch = true;
    feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode = true;

    bool isCooperativeKernel = false;
    bool disableEuFusion = true;
    bool disableOverdispatch = true;
    feProperties.setPropertiesAll(isCooperativeKernel, disableEuFusion, disableOverdispatch);
    EXPECT_TRUE(feProperties.isDirty());
    EXPECT_EQ(0, feProperties.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, feProperties.disableEUFusion.value);
    EXPECT_EQ(1, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    feProperties.resetState();
    EXPECT_FALSE(feProperties.isDirty());
    EXPECT_EQ(-1, feProperties.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, feProperties.disableEUFusion.value);
    EXPECT_EQ(-1, feProperties.disableOverdispatch.value);
    EXPECT_EQ(-1, feProperties.singleSliceDispatchCcsMode.value);

    EXPECT_TRUE(feProperties.propertiesSupportLoaded);
    EXPECT_TRUE(feProperties.frontEndPropertiesSupport.computeDispatchAllWalker);
    EXPECT_TRUE(feProperties.frontEndPropertiesSupport.disableEuFusion);
    EXPECT_TRUE(feProperties.frontEndPropertiesSupport.disableOverdispatch);
    EXPECT_TRUE(feProperties.frontEndPropertiesSupport.singleSliceDispatchCcsMode);
}

TEST(StreamPropertiesTests, whenSettingPipelineSelectPropertiesThenCorrectValueIsSet) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    StreamProperties properties;
    properties.initSupport(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};
    productHelper.fillPipelineSelectPropertiesSupportStructure(pipelineSelectPropertiesSupport, *defaultHwInfo);

    for (auto modeSelected : ::testing::Bool()) {
        for (auto systolicMode : ::testing::Bool()) {
            properties.pipelineSelect.setPropertiesAll(modeSelected, systolicMode);

            EXPECT_EQ(modeSelected, properties.pipelineSelect.modeSelected.value);
            if (pipelineSelectPropertiesSupport.systolicMode) {
                EXPECT_EQ(systolicMode, properties.pipelineSelect.systolicMode.value);
            } else {
                EXPECT_EQ(-1, properties.pipelineSelect.systolicMode.value);
            }
        }
    }
}

TEST(StreamPropertiesTests, givenModeSelectPipelineSelectPropertyWhenSettingChangedPropertyAndCheckIfDirtyThenExpectDirtyState) {
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;

    constexpr bool constState = false;
    bool changingState = false;
    pipeProperties.setPropertiesAll(changingState, constState);

    EXPECT_TRUE(pipeProperties.isDirty());

    changingState = !changingState;
    pipeProperties.setPropertiesAll(changingState, constState);

    EXPECT_TRUE(pipeProperties.isDirty());
}

TEST(StreamPropertiesTests, givenSetAllPipelineSelectPropertiesWhenResettingStateThenResetValuesAndDirtyKeepSupportFlagLoaded) {
    MockPipelineSelectProperties psProperties{};
    psProperties.propertiesSupportLoaded = true;
    psProperties.pipelineSelectPropertiesSupport.systolicMode = true;

    bool modeSelected = false;
    bool systolicMode = true;
    psProperties.setPropertiesAll(modeSelected, systolicMode);
    EXPECT_TRUE(psProperties.isDirty());
    EXPECT_EQ(0, psProperties.modeSelected.value);
    EXPECT_EQ(1, psProperties.systolicMode.value);

    psProperties.resetState();
    EXPECT_FALSE(psProperties.isDirty());
    EXPECT_EQ(-1, psProperties.modeSelected.value);
    EXPECT_EQ(-1, psProperties.systolicMode.value);

    EXPECT_TRUE(psProperties.propertiesSupportLoaded);
    EXPECT_TRUE(psProperties.pipelineSelectPropertiesSupport.systolicMode);
}

TEST(StreamPropertiesTests, givenSystolicModePipelineSelectPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = false;

    bool systolicMode = false;
    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(-1, pipeProperties.systolicMode.value);

    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;
    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.systolicMode.value);

    systolicMode = true;
    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);

    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.systolicMode.value);
}

TEST(StreamPropertiesTests, givenSystolicModePipelineSelectPropertyWhenCopyingPropertyAndCheckIfDirtyThenExpectCorrectState) {
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;
    pipeProperties.pipelineSelectPropertiesSupport.systolicMode = true;

    bool systolicMode = false;
    pipeProperties.setPropertySystolicMode(systolicMode);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.systolicMode.value);

    MockPipelineSelectProperties pipePropertiesCopy{};
    pipePropertiesCopy.copyPropertiesSystolicMode(pipeProperties);
    EXPECT_TRUE(pipePropertiesCopy.isDirty());
    EXPECT_EQ(0, pipePropertiesCopy.systolicMode.value);

    pipePropertiesCopy.copyPropertiesSystolicMode(pipeProperties);
    EXPECT_FALSE(pipePropertiesCopy.isDirty());
    EXPECT_EQ(0, pipePropertiesCopy.systolicMode.value);

    systolicMode = true;
    pipeProperties.setPropertySystolicMode(systolicMode);

    pipePropertiesCopy.copyPropertiesSystolicMode(pipeProperties);
    EXPECT_TRUE(pipePropertiesCopy.isDirty());
    EXPECT_EQ(1, pipePropertiesCopy.systolicMode.value);
}

TEST(StreamPropertiesTests, givenModeSelectedMediaSamplerClockGatePipelineSelectPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    bool clearDirtyState = false;
    MockPipelineSelectProperties pipeProperties{};
    pipeProperties.propertiesSupportLoaded = true;

    bool modeSelected = false;
    pipeProperties.setPropertiesModeSelected(modeSelected, clearDirtyState);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.modeSelected.value);

    pipeProperties.setPropertiesModeSelected(modeSelected, clearDirtyState);
    EXPECT_FALSE(pipeProperties.isDirty());

    modeSelected = true;
    pipeProperties.setPropertiesModeSelected(modeSelected, clearDirtyState);
    EXPECT_TRUE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.modeSelected.value);

    pipeProperties.setPropertiesModeSelected(modeSelected, clearDirtyState);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(1, pipeProperties.modeSelected.value);

    clearDirtyState = true;
    modeSelected = false;
    pipeProperties.setPropertiesModeSelected(modeSelected, clearDirtyState);
    EXPECT_FALSE(pipeProperties.isDirty());
    EXPECT_EQ(0, pipeProperties.modeSelected.value);
}

TEST(StreamPropertiesTests, givenStateBaseAddressSupportFlagStateWhenSettingPropertyAndCheckIfDirtyThenExpectCleanStateForNotSupportedAndDirtyForSupported) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = false;

    sbaProperties.setPropertiesAll(-1, 1, -1, -1, -1, -1, -1, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());

    EXPECT_EQ(-1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);

    sbaProperties.setPropertiesAll(1, 1, -1, -1, -1, -1, -1, -1, -1);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_TRUE(sbaProperties.statelessMocs.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolSize.isDirty);

    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = true;
    sbaProperties.setPropertiesAll(-1, 2, 2, -1, -1, -1, -1, -1, -1);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_FALSE(sbaProperties.statelessMocs.isDirty);
    EXPECT_TRUE(sbaProperties.bindingTablePoolBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.bindingTablePoolSize.isDirty);

    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(2, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);

    sbaProperties.setPropertiesAll(1, 2, 2, -1, -1, -1, -1, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());

    sbaProperties.setPropertiesAll(0, 3, 2, -1, -1, -1, -1, -1 - 1, -1);
    EXPECT_TRUE(sbaProperties.isDirty());

    EXPECT_EQ(0, sbaProperties.statelessMocs.value);
    EXPECT_EQ(3, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);

    sbaProperties.setPropertiesAll(0, 3, 3, -1, -1, -1, -1, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());

    EXPECT_EQ(0, sbaProperties.statelessMocs.value);
    EXPECT_EQ(3, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(3u, sbaProperties.bindingTablePoolSize.value);

    sbaProperties.setPropertiesAll(0, 3, -1, -1, -1, -1, -1, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());

    MockStateBaseAddressProperties copySbaProperties{};

    copySbaProperties.copyPropertiesAll(sbaProperties);
    EXPECT_TRUE(copySbaProperties.isDirty());

    EXPECT_EQ(0, copySbaProperties.statelessMocs.value);
    EXPECT_EQ(3, copySbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(3u, copySbaProperties.bindingTablePoolSize.value);

    sbaProperties.copyPropertiesAll(copySbaProperties);
    EXPECT_FALSE(sbaProperties.isDirty());
}

TEST(StreamPropertiesTests, givenStateBaseAddressSupportFlagDefaultValueWhenSettingPropertyAndCheckIfDirtyThenExpectValueSetForSupportedAndCleanForNotSupported) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    StateBaseAddressPropertiesSupport sbaPropertiesSupport = {};
    productHelper.fillStateBaseAddressPropertiesSupportStructure(sbaPropertiesSupport);

    StateBaseAddressProperties sbaProperties{};
    sbaProperties.initSupport(*mockExecutionEnvironment.rootDeviceEnvironments[0]);

    sbaProperties.setPropertiesAll(2, 3, 3, -1, -1, -1, -1, -1, -1);

    EXPECT_EQ(2, sbaProperties.statelessMocs.value);

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
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = false;

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, -1, -1, -1, -1, -1);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(10, sbaProperties.surfaceStateBaseAddress.value);

    EXPECT_TRUE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, -1, -1, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(20u, sbaProperties.surfaceStateSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, 30, -1, -1, -1);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(30, sbaProperties.dynamicStateBaseAddress.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_TRUE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, 30, 40, -1, -1);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(40u, sbaProperties.dynamicStateSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, 30, 40, 50, -1);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(50, sbaProperties.indirectObjectBaseAddress.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_TRUE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, 30, 40, 50, 60);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(60u, sbaProperties.indirectObjectSize.value);

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);

    sbaProperties.setPropertiesAll(-1, -1, 10, 10, 20, 30, 40, 50, 60);
    EXPECT_FALSE(sbaProperties.isDirty());

    EXPECT_FALSE(sbaProperties.surfaceStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.surfaceStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.dynamicStateSize.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectBaseAddress.isDirty);
    EXPECT_FALSE(sbaProperties.indirectObjectSize.isDirty);
}

TEST(StreamPropertiesTests, givenStatelessMocsStateBaseAddressPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    int32_t statelessMocs = -1;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(-1, sbaProperties.statelessMocs.value);

    statelessMocs = 0;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);

    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);

    statelessMocs = 1;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);

    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);

    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
}

TEST(StreamPropertiesTests, givenStatelessMocsStateBaseAddressPropertyWhenCopyingPropertyAndCheckIfDirtyThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    int32_t statelessMocs = 0;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);

    MockStateBaseAddressProperties sbaPropertiesCopy{};
    sbaPropertiesCopy.copyPropertiesStatelessMocs(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(0, sbaPropertiesCopy.statelessMocs.value);

    sbaPropertiesCopy.copyPropertiesStatelessMocs(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(0, sbaPropertiesCopy.statelessMocs.value);

    statelessMocs = 1;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);

    sbaPropertiesCopy.copyPropertiesStatelessMocs(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.statelessMocs.value);
}

TEST(StreamPropertiesTests, givenIndirectHeapAndStatelessMocsStateBaseAddressPropertyWhenCopyingPropertyAndCheckIfDirtyThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    int32_t statelessMocs = 0;
    int64_t indirectObjectBaseAddress = 1;
    size_t indirectObjectSize = 1;

    sbaProperties.setPropertyStatelessMocs(statelessMocs);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.statelessMocs.value);

    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.indirectObjectSize.value);

    MockStateBaseAddressProperties sbaPropertiesCopy{};
    sbaPropertiesCopy.copyPropertiesStatelessMocsIndirectState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(0, sbaPropertiesCopy.statelessMocs.value);
    EXPECT_EQ(1, sbaPropertiesCopy.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.indirectObjectSize.value);

    sbaPropertiesCopy.copyPropertiesStatelessMocsIndirectState(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(0, sbaPropertiesCopy.statelessMocs.value);
    EXPECT_EQ(1, sbaPropertiesCopy.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.indirectObjectSize.value);

    statelessMocs = 1;
    sbaProperties.setPropertyStatelessMocs(statelessMocs);

    sbaPropertiesCopy.copyPropertiesStatelessMocsIndirectState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.statelessMocs.value);
    EXPECT_EQ(1, sbaPropertiesCopy.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.indirectObjectSize.value);

    indirectObjectBaseAddress = 2;
    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    sbaPropertiesCopy.copyPropertiesStatelessMocsIndirectState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.statelessMocs.value);
    EXPECT_EQ(2, sbaPropertiesCopy.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.indirectObjectSize.value);

    indirectObjectSize = 2;
    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    sbaPropertiesCopy.copyPropertiesStatelessMocsIndirectState(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.statelessMocs.value);
    EXPECT_EQ(2, sbaPropertiesCopy.indirectObjectBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.indirectObjectSize.value);
}

TEST(StreamPropertiesTests, givenBindingTableAndSurfaceStateBaseAddressStateBaseAddressPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    int64_t bindingTablePoolBaseAddress = 0;
    size_t bindingTablePoolSize = 0;
    int64_t surfaceStateBaseAddress = 0;
    size_t surfaceStateSize = 0;

    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = false;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(static_cast<size_t>(-1), sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = true;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    bindingTablePoolBaseAddress = 1;
    bindingTablePoolSize = 1;
    surfaceStateBaseAddress = 1;
    surfaceStateSize = 1;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.surfaceStateSize.value);

    surfaceStateSize = 2;
    bindingTablePoolSize = 2;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);

    bindingTablePoolBaseAddress = 2;
    surfaceStateBaseAddress = 2;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(2, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);
}

TEST(StreamPropertiesTests, givenSurfaceStateBaseAddressStateBaseAddressPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    int64_t surfaceStateBaseAddress = 0;
    size_t surfaceStateSize = 0;

    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.surfaceStateSize.value);

    surfaceStateBaseAddress = 1;
    surfaceStateSize = 1;
    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.surfaceStateSize.value);

    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.surfaceStateSize.value);

    surfaceStateSize = 2;
    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);

    surfaceStateBaseAddress = 2;
    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);
}

TEST(StreamPropertiesTests, givenDynamicStateBaseAddressStateBaseAddressPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    int64_t dynamicStateBaseAddress = 0;
    size_t dynamicStateSize = 0;

    MockStateBaseAddressProperties sbaProperties{};

    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.dynamicStateSize.value);

    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.dynamicStateSize.value);

    dynamicStateBaseAddress = 1;
    dynamicStateSize = 1;
    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.dynamicStateSize.value);

    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.dynamicStateSize.value);

    dynamicStateSize = 2;
    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);

    dynamicStateBaseAddress = 2;
    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);
}

TEST(StreamPropertiesTests, givenIndirectObjectBaseAddressStateBaseAddressPropertyWhenSettingPropertyAndCheckIfSupportedThenExpectCorrectState) {
    int64_t indirectObjectBaseAddress = 0;
    size_t indirectObjectSize = 0;

    MockStateBaseAddressProperties sbaProperties{};

    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.indirectObjectSize.value);

    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(0, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(0u, sbaProperties.indirectObjectSize.value);

    indirectObjectBaseAddress = 1;
    indirectObjectSize = 1;
    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.indirectObjectSize.value);

    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.indirectObjectSize.value);

    indirectObjectSize = 2;
    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.indirectObjectSize.value);

    indirectObjectBaseAddress = 2;
    sbaProperties.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.indirectObjectSize.value);
}

TEST(StreamPropertiesTests, givenSetAllStateBaseAddressPropertiesWhenResettingStateThenResetValuesAndDirtyKeepSupportFlagLoaded) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = true;

    int32_t statelessMocs = 1;
    int64_t bindingTablePoolBaseAddress = 2;
    size_t bindingTablePoolSize = 3;
    int64_t surfaceStateBaseAddress = 4;
    size_t surfaceStateSize = 5;
    int64_t dynamicStateBaseAddress = 6;
    size_t dynamicStateSize = 7;
    int64_t indirectObjectBaseAddress = 8;
    size_t indirectObjectSize = 9;

    sbaProperties.setPropertiesAll(statelessMocs,
                                   bindingTablePoolBaseAddress, bindingTablePoolSize,
                                   surfaceStateBaseAddress, surfaceStateSize,
                                   dynamicStateBaseAddress, dynamicStateSize,
                                   indirectObjectBaseAddress, indirectObjectSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(2, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(3u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(4, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(5u, sbaProperties.surfaceStateSize.value);
    EXPECT_EQ(6, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(7u, sbaProperties.dynamicStateSize.value);
    EXPECT_EQ(8, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(9u, sbaProperties.indirectObjectSize.value);

    sbaProperties.resetState();
    EXPECT_FALSE(sbaProperties.isDirty());
    EXPECT_EQ(-1, sbaProperties.statelessMocs.value);
    EXPECT_EQ(-1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(-1, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, sbaProperties.surfaceStateSize.value);
    EXPECT_EQ(-1, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, sbaProperties.dynamicStateSize.value);
    EXPECT_EQ(-1, sbaProperties.indirectObjectBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, sbaProperties.indirectObjectSize.value);

    EXPECT_TRUE(sbaProperties.propertiesSupportLoaded);
    EXPECT_TRUE(sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress);
}

TEST(StreamPropertiesTests, givenAllStreamPropertiesSetWhenAllStreamPropertiesResetStateThenAllValuesBringToInitValue) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    StreamProperties globalStreamProperties{};
    globalStreamProperties.initSupport(rootDeviceEnvironment);

    uint32_t grfNumber = 128;
    int32_t threadArbitration = 1;
    globalStreamProperties.stateComputeMode.setPropertiesAll(false, grfNumber, threadArbitration, PreemptionMode::Initial, false);

    bool isCooperativeKernel = false;
    bool disableEuFusion = true;
    bool disableOverdispatch = true;
    globalStreamProperties.frontEndState.setPropertiesAll(isCooperativeKernel, disableEuFusion, disableOverdispatch);

    bool modeSelected = false;
    bool systolicMode = true;
    globalStreamProperties.pipelineSelect.setPropertiesAll(modeSelected, systolicMode);

    int32_t statelessMocs = 1;
    int64_t bindingTablePoolBaseAddress = 2;
    size_t bindingTablePoolSize = 3;
    int64_t surfaceStateBaseAddress = 4;
    size_t surfaceStateSize = 5;
    int64_t dynamicStateBaseAddress = 6;
    size_t dynamicStateSize = 7;
    int64_t indirectObjectBaseAddress = 8;
    size_t indirectObjectSize = 9;
    globalStreamProperties.stateBaseAddress.setPropertiesAll(statelessMocs,
                                                             bindingTablePoolBaseAddress, bindingTablePoolSize,
                                                             surfaceStateBaseAddress, surfaceStateSize,
                                                             dynamicStateBaseAddress, dynamicStateSize,
                                                             indirectObjectBaseAddress, indirectObjectSize);

    globalStreamProperties.resetState();

    EXPECT_EQ(-1, globalStreamProperties.stateComputeMode.largeGrfMode.value);
    EXPECT_EQ(-1, globalStreamProperties.stateComputeMode.threadArbitrationPolicy.value);
    EXPECT_EQ(-1, globalStreamProperties.stateComputeMode.isCoherencyRequired.value);
    EXPECT_EQ(-1, globalStreamProperties.stateComputeMode.devicePreemptionMode.value);

    EXPECT_EQ(-1, globalStreamProperties.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, globalStreamProperties.frontEndState.disableEUFusion.value);
    EXPECT_EQ(-1, globalStreamProperties.frontEndState.disableOverdispatch.value);
    EXPECT_EQ(-1, globalStreamProperties.frontEndState.singleSliceDispatchCcsMode.value);

    EXPECT_EQ(-1, globalStreamProperties.pipelineSelect.modeSelected.value);
    EXPECT_EQ(-1, globalStreamProperties.pipelineSelect.systolicMode.value);

    EXPECT_EQ(-1, globalStreamProperties.stateBaseAddress.statelessMocs.value);
    EXPECT_EQ(-1, globalStreamProperties.stateBaseAddress.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, globalStreamProperties.stateBaseAddress.bindingTablePoolSize.value);
    EXPECT_EQ(-1, globalStreamProperties.stateBaseAddress.surfaceStateBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, globalStreamProperties.stateBaseAddress.surfaceStateSize.value);
    EXPECT_EQ(-1, globalStreamProperties.stateBaseAddress.dynamicStateBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, globalStreamProperties.stateBaseAddress.dynamicStateSize.value);
    EXPECT_EQ(-1, globalStreamProperties.stateBaseAddress.indirectObjectBaseAddress.value);
    EXPECT_EQ(StreamPropertySizeT::initValue, globalStreamProperties.stateBaseAddress.indirectObjectSize.value);
}

TEST(StreamPropertiesTests, givenBindingTableSurfaceStateStateBaseAddressPropertyWhenCopyingPropertiesAndCheckIfDirtyThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;
    sbaProperties.stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress = true;

    int64_t bindingTableBaseAddress = 1;
    size_t bindingTableSize = 1;

    int64_t surfaceStateBaseAddress = 2;
    size_t surfaceStateSize = 2;

    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTableBaseAddress, bindingTableSize, surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(1, sbaProperties.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaProperties.bindingTablePoolSize.value);
    EXPECT_EQ(2, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);

    MockStateBaseAddressProperties sbaPropertiesCopy{};
    sbaPropertiesCopy.copyPropertiesBindingTableSurfaceState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.bindingTablePoolSize.value);
    EXPECT_EQ(2, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);

    sbaPropertiesCopy.copyPropertiesBindingTableSurfaceState(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(1, sbaPropertiesCopy.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.bindingTablePoolSize.value);
    EXPECT_EQ(2, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);

    bindingTableBaseAddress = 2;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTableBaseAddress, bindingTableSize, surfaceStateBaseAddress, surfaceStateSize);
    sbaPropertiesCopy.copyPropertiesBindingTableSurfaceState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaPropertiesCopy.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.bindingTablePoolSize.value);
    EXPECT_EQ(2, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);

    surfaceStateBaseAddress = 3;
    sbaProperties.setPropertiesBindingTableSurfaceState(bindingTableBaseAddress, bindingTableSize, surfaceStateBaseAddress, surfaceStateSize);
    sbaPropertiesCopy.copyPropertiesBindingTableSurfaceState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaPropertiesCopy.bindingTablePoolBaseAddress.value);
    EXPECT_EQ(1u, sbaPropertiesCopy.bindingTablePoolSize.value);
    EXPECT_EQ(3, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);
}

TEST(StreamPropertiesTests, givenSurfaceStateStateBaseAddressPropertyWhenCopyingPropertiesAndCheckIfDirtyThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    int64_t surfaceStateBaseAddress = 2;
    size_t surfaceStateSize = 2;

    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.surfaceStateSize.value);

    MockStateBaseAddressProperties sbaPropertiesCopy{};
    sbaPropertiesCopy.copyPropertiesSurfaceState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);

    sbaPropertiesCopy.copyPropertiesSurfaceState(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);

    surfaceStateBaseAddress = 3;
    sbaProperties.setPropertiesSurfaceState(surfaceStateBaseAddress, surfaceStateSize);
    sbaPropertiesCopy.copyPropertiesSurfaceState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(3, sbaPropertiesCopy.surfaceStateBaseAddress.value);
    EXPECT_EQ(2u, sbaPropertiesCopy.surfaceStateSize.value);
}

TEST(StreamPropertiesTests, givenDynamicStateStateBaseAddressPropertyWhenCopyingPropertiesAndCheckIfDirtyThenExpectCorrectState) {
    MockStateBaseAddressProperties sbaProperties{};
    sbaProperties.propertiesSupportLoaded = true;

    int64_t dynamicStateBaseAddress = 2;
    size_t dynamicStateSize = 2;

    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    EXPECT_TRUE(sbaProperties.isDirty());
    EXPECT_EQ(2, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);

    MockStateBaseAddressProperties sbaPropertiesCopy{};
    sbaPropertiesCopy.copyPropertiesDynamicState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);

    sbaPropertiesCopy.copyPropertiesDynamicState(sbaProperties);
    EXPECT_FALSE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(2, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);

    dynamicStateBaseAddress = 4;
    sbaProperties.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    sbaPropertiesCopy.copyPropertiesDynamicState(sbaProperties);
    EXPECT_TRUE(sbaPropertiesCopy.isDirty());
    EXPECT_EQ(4, sbaProperties.dynamicStateBaseAddress.value);
    EXPECT_EQ(2u, sbaProperties.dynamicStateSize.value);
}
