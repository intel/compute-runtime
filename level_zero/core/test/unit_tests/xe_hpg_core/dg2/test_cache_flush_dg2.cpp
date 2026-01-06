/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using CacheFlushDG2Tests = Test<DeviceFixture>;

HWTEST2_F(CacheFlushDG2Tests, givenCommandStreamWithSingleL3RangeAndNonZeroPostSyncAddressWhenFlushGpuCacheIsCalledThenPostSyncOperationIsSetForL3ControlAndUnTypedDataPortCacheFlushIsSet, IsDG2) {
    using GfxFamily = typename NEO::GfxFamilyMapper<FamilyType::gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(cmdStream->getCpuBase(), 0), cmdStream->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
    auto l3Control = genCmdCast<L3_CONTROL *>(*itor);
    EXPECT_TRUE(l3Control->getUnTypedDataPortCacheFlush());
}

} // namespace ult
} // namespace L0
