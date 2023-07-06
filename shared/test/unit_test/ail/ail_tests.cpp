/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using IsSKL = IsProduct<IGFX_SKYLAKE>;
using IsDG2 = IsProduct<IGFX_DG2>;
using IsHostPtrTrackingDisabled = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_GEN11LP_CORE>;

using AILTests = ::testing::Test;
template <PRODUCT_FAMILY productFamily>
class AILMock : public AILConfigurationHw<productFamily> {
  public:
    using AILConfiguration::isKernelHashCorrect;
    using AILConfiguration::processName;
    using AILConfiguration::sourcesContain;
};

HWTEST2_F(AILTests, givenInitializedTemplateWhenGetAILConfigurationThenNullptrIsNotReturned, IsSKL) {
    auto ailConfiguration = AILConfiguration::get(productFamily);
    EXPECT_NE(nullptr, ailConfiguration);
}

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithBlenderIsCalledThenFP64SupportIsEnabled, IsAtLeastGen12lp) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "blender";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.ftrSupportsFP64 = false;

    ailConfiguration->apply(rtTable);

    EXPECT_EQ(rtTable.ftrSupportsFP64, true);
}

HWTEST2_F(AILTests, whenCheckingIfSourcesContainKernelThenCorrectResultIsReturned, IsAtLeastGen12lp) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    AILMock<productFamily> ail;
    ailConfigurationTable[productFamily] = &ail;
    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    std::string kernelSources = R"( 
__kernel void CopyBufferToBufferLeftLeftover(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}

__kernel void CopyBufferToBufferMiddle(
    const __global uint* pSrc,
    __global uint* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst += dstOffsetInBytes >> 2;
    pSrc += srcOffsetInBytes >> 2;
    uint4 loaded = vload4(gid, pSrc);
    vstore4(loaded, gid, pDst);)";

    EXPECT_TRUE(ail.sourcesContain(kernelSources, "CopyBufferToBufferMiddle"));
    EXPECT_FALSE(ail.sourcesContain(kernelSources, "CopyBufferToBufferMiddleStateless"));
}

HWTEST2_F(AILTests, whenCheckingIsKernelHashCorrectThenCorrectResultIsReturned, IsAtLeastGen12lp) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    AILMock<productFamily> ail;
    ailConfigurationTable[productFamily] = &ail;
    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    std::string kernelSources = R"( 
__kernel void CopyBufferToBufferLeftLeftover(
    const __global uchar* pSrc,
    __global uchar* pDst,
    uint srcOffsetInBytes,
    uint dstOffsetInBytes)
{
    unsigned int gid = get_global_id(0);
    pDst[ gid + dstOffsetInBytes ] = pSrc[ gid + srcOffsetInBytes ];
}
)";

    auto expectedHash = 0xafeba928e880fd89;

    // If this check fails, probably hash algorithm has been changed.
    // In this case we must regenerate hashes in AIL applications kernels
    EXPECT_TRUE(ail.isKernelHashCorrect(kernelSources, expectedHash));

    kernelSources.insert(0, "text");
    EXPECT_FALSE(ail.isKernelHashCorrect(kernelSources, expectedHash));
}

HWTEST2_F(AILTests, whenModifyKernelIfRequiredIsCalledThenDontChangeKernelSources, IsAtLeastGen12lp) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    AILMock<productFamily> ail;
    ailConfigurationTable[productFamily] = &ail;
    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    std::string kernelSources = "example_kernel(){}";
    auto copyKernel = kernelSources;

    ail.modifyKernelIfRequired(kernelSources);

    EXPECT_STREQ(copyKernel.c_str(), kernelSources.c_str());
}

HWTEST2_F(AILTests, givenPreGen12AndProcessNameIsResolveWhenApplyWithDavinciResolveThenHostPtrTrackingIsDisabled, IsHostPtrTrackingDisabled) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "resolve";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.hostPtrTrackingEnabled = true;

    ailConfiguration->apply(rtTable);

    EXPECT_FALSE(rtTable.hostPtrTrackingEnabled);
}

HWTEST2_F(AILTests, givenPreGen12AndAndProcessNameIsNotResolveWhenApplyWithDavinciResolveThenHostPtrTrackingIsEnabled, IsHostPtrTrackingDisabled) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "usualProcessName";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.hostPtrTrackingEnabled = true;

    ailConfiguration->apply(rtTable);

    EXPECT_TRUE(rtTable.hostPtrTrackingEnabled);
}

class MockAILConfiguration : public AILConfiguration {
  public:
    bool initProcessExecutableName() override {
        initCalled = true;
        return true;
    }
    bool initCalled = false;
    void modifyKernelIfRequired(std::string &kernel) override {}

    bool isFallbackToPatchtokensRequired(const std::string &kernelSources) override {
        return false;
    }

  protected:
    void applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) override {}
};

HWTEST_F(AILTests, whenAilIsDisabledByDebugVariableThenAilIsNotInitialized) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.EnableAIL.set(false);

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    MockAILConfiguration ailConfig;
    ailConfigurationTable[productFamily] = &ailConfig;

    HardwareInfo hwInfo{};
    hwInfo.platform.eProductFamily = productFamily;
    hwInfo.platform.eRenderCoreFamily = renderCoreFamily;

    NEO::MockExecutionEnvironment executionEnvironment{&hwInfo, true, 1};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAilConfiguration();

    EXPECT_EQ(false, ailConfig.initCalled);
}

HWTEST_F(AILTests, whenAilIsEnabledByDebugVariableThenAilIsInitialized) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.EnableAIL.set(true);

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    MockAILConfiguration ailConfig;
    ailConfigurationTable[productFamily] = &ailConfig;

    HardwareInfo hwInfo{};
    hwInfo.platform.eProductFamily = productFamily;
    hwInfo.platform.eRenderCoreFamily = renderCoreFamily;

    NEO::MockExecutionEnvironment executionEnvironment{&hwInfo, true, 1};
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initAilConfiguration();

    EXPECT_EQ(true, ailConfig.initCalled);
}

} // namespace NEO
