/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using IsSKL = IsProduct<IGFX_SKYLAKE>;
using IsDG2 = IsProduct<IGFX_DG2>;

using AILTests = ::testing::Test;
template <PRODUCT_FAMILY productFamily>
class AILMock : public AILConfigurationHw<productFamily> {
  public:
    using AILConfiguration::isKernelHashCorrect;
    using AILConfiguration::processName;
    using AILConfiguration::sourcesContainKernel;
};

HWTEST2_F(AILTests, givenUninitializedTemplateWhenGetAILConfigurationThenNullptrIsReturned, IsSKL) {
    auto ailConfiguration = AILConfiguration::get(productFamily);

    ASSERT_EQ(nullptr, ailConfiguration);
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

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithWondershareFilmora11IsCalledThenBlitterSupportIsDisabled, IsDG2) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "Wondershare Filmora 11";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.blitterOperationsSupported = true;

    ailConfiguration->apply(rtTable);

    EXPECT_EQ(rtTable.blitterOperationsSupported, false);
}

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithWondershareFilmora11perf_checkSubprocessIsCalledThenBlitterSupportIsDisabled, IsDG2) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "perf_check";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.blitterOperationsSupported = true;

    ailConfiguration->apply(rtTable);

    EXPECT_EQ(rtTable.blitterOperationsSupported, false);
}

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithWondershareFilmora11tlb_player_guiSubprocessIsCalledThenBlitterSupportIsDisabled, IsDG2) {
    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);

    AILMock<productFamily> ailTemp;
    ailTemp.processName = "tlb_player_gui";
    ailConfigurationTable[productFamily] = &ailTemp;

    auto ailConfiguration = AILConfiguration::get(productFamily);
    ASSERT_NE(nullptr, ailConfiguration);

    NEO::RuntimeCapabilityTable rtTable = {};
    rtTable.blitterOperationsSupported = true;

    ailConfiguration->apply(rtTable);

    EXPECT_EQ(rtTable.blitterOperationsSupported, false);
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

    EXPECT_TRUE(ail.sourcesContainKernel(kernelSources, "CopyBufferToBufferMiddle"));
    EXPECT_FALSE(ail.sourcesContainKernel(kernelSources, "CopyBufferToBufferMiddleStateless"));
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

} // namespace NEO
