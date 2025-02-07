/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
using IsDG2 = IsProduct<IGFX_DG2>;

using AILTests = ::testing::Test;

TEST(AILTests, whenAILConfigurationCreateFunctionIsCalledWithUnknownGfxCoreThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, AILConfiguration::create(IGFX_UNKNOWN));
}

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithBlenderIsCalledThenFP64SupportIsEnabled, MatchAny) {
    AILWhitebox<productFamily> ail;
    ail.processName = "blender";

    HardwareInfo hwInfo = {};
    NEO::RuntimeCapabilityTable &rtTable = hwInfo.capabilityTable;
    rtTable.ftrSupportsFP64 = false;

    ail.apply(hwInfo);

    EXPECT_EQ(rtTable.ftrSupportsFP64, true);
}

HWTEST2_F(AILTests, givenInitilizedTemplateWhenApplyWithAdobePremiereProIsCalledThenPreferredPlatformNameIsSet, MatchAny) {
    AILWhitebox<productFamily> ail;
    ail.processName = "Adobe Premiere Pro";

    HardwareInfo hwInfo = {};
    NEO::RuntimeCapabilityTable &rtTable = hwInfo.capabilityTable;
    rtTable.preferredPlatformName = nullptr;

    ail.apply(hwInfo);

    EXPECT_NE(nullptr, rtTable.preferredPlatformName);
    EXPECT_STREQ("Intel(R) OpenCL", rtTable.preferredPlatformName);
}

HWTEST2_F(AILTests, whenCheckingIfSourcesContainKernelThenCorrectResultIsReturned, MatchAny) {
    AILWhitebox<productFamily> ail;

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

HWTEST2_F(AILTests, whenCheckingIsKernelHashCorrectThenCorrectResultIsReturned, MatchAny) {
    AILWhitebox<productFamily> ail;

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

HWTEST2_F(AILTests, whenModifyKernelIfRequiredIsCalledThenDontChangeKernelSources, MatchAny) {
    AILWhitebox<productFamily> ail;

    std::string kernelSources = "example_kernel(){}";
    auto copyKernel = kernelSources;

    ail.modifyKernelIfRequired(kernelSources);

    EXPECT_STREQ(copyKernel.c_str(), kernelSources.c_str());
}

HWTEST2_F(AILTests, givenAilWhenCheckingContextSyncFlagRequiredThenExpectFalse, MatchAny) {
    AILWhitebox<productFamily> ail;
    ail.processName = "other";
    EXPECT_FALSE(ail.isContextSyncFlagRequired());
}

HWTEST2_F(AILTests, givenAilWhenCheckingOverfetchDisableRequiredThenExpectFalse, MatchAny) {
    AILWhitebox<productFamily> ail;
    ail.processName = "other";
    EXPECT_FALSE(ail.is256BPrefetchDisableRequired());
}

HWTEST2_F(AILTests, givenAilWhenCheckingDrainHostptrsRequiredThenExpectTrue, MatchAny) {
    AILWhitebox<productFamily> ail;
    ail.processName = "other";
    EXPECT_TRUE(ail.drainHostptrs());
}

HWTEST2_F(AILTests, givenAilWhenGetMicrosecondResolutionCalledThenCorrectValueReturned, MatchAny) {
    AILWhitebox<productFamily> ail;
    EXPECT_EQ(ail.getMicrosecondResolution(), microsecondAdjustment);
}

} // namespace NEO
