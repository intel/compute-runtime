/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush.inl"
#include "shared/source/helpers/l3_range.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using CacheFlushTests = Test<DeviceFixture>;

HWTEST2_F(CacheFlushTests, GivenCommandStreamWithSingleL3RangeAndNonZeroPostSyncAddressWhenFlushGpuCacheIsCalledThenPostSyncOperationIsSetForL3Control, IsDG1) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    LinearStream *cmdStream = commandList->commandContainer.getCommandStream();
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1000;
    uint64_t postSyncAddress = 0x1200;

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(
        gpuAddress, size,
        GfxFamily::L3_FLUSH_ADDRESS_RANGE::
            L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    NEO::flushGpuCache<GfxFamily>(cmdStream, ranges, postSyncAddress,
                                  hardwareInfo);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(cmdStream->getCpuBase(), 0), cmdStream->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CacheFlushTests, GivenCommandStreamWithMultipleL3RangeAndUsePostSyncIsSetToTrueWhenGetSizeNeededToFlushGpuCacheIsCalledThenCorrectSizeIsReturned, IsDG1) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    uint64_t gpuAddress = 0x1200;
    size_t size = 0x1000;

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(
        gpuAddress, size,
        GfxFamily::L3_FLUSH_ADDRESS_RANGE::
            L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    EXPECT_NE(0u, ranges.size());
    size_t ret = NEO::getSizeNeededToFlushGpuCache<GfxFamily>(ranges, true);
    size_t expected = ranges.size() * sizeof(L3_CONTROL);
    EXPECT_EQ(ret, expected);
}

HWTEST2_F(CacheFlushTests, GivenCommandStreamWithMultipleL3RangeAndUsePostSyncIsSetToFalseWhenGetSizeNeededToFlushGpuCacheIsCalledThenCorrectSizeIsReturned, IsDG1) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    uint64_t gpuAddress = 0x1200;
    size_t size = 0x1000;

    L3RangesVec ranges;
    ranges.push_back(L3Range::fromAddressSizeWithPolicy(
        gpuAddress, size,
        GfxFamily::L3_FLUSH_ADDRESS_RANGE::
            L3_FLUSH_EVICTION_POLICY_FLUSH_L3_WITH_EVICTION));
    EXPECT_NE(0u, ranges.size());
    size_t ret = NEO::getSizeNeededToFlushGpuCache<GfxFamily>(ranges, false);
    size_t expected = ranges.size() * sizeof(L3_CONTROL);
    EXPECT_EQ(ret, expected);
}

} // namespace ult
} // namespace L0
