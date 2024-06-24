/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/sysman/source/api/memory/windows/sysman_os_memory_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/memory/windows/mock_memory.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanProductHelperMemoryFixture = SysmanDeviceMemoryFixture;

HWTEST2_F(SysmanProductHelperMemoryFixture, GivenValidMemoryHandleWhenGettingBandwidthThenCallSucceeds, IsAtMostDg2) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        ze_result_t result = zesMemoryGetBandwidth(handle, &bandwidth);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_EQ(bandwidth.maxBandwidth, static_cast<uint64_t>(pKmdSysManager->mockMemoryMaxBandwidth) * mbpsToBytesPerSecond);
        EXPECT_EQ(bandwidth.readCounter, pKmdSysManager->mockMemoryCurrentBandwidthRead);
        EXPECT_EQ(bandwidth.writeCounter, pKmdSysManager->mockMemoryCurrentBandwidthWrite);
        EXPECT_GT(bandwidth.timestamp, 0u);

        std::vector<uint32_t> requestId = {KmdSysman::Requests::Memory::MaxBandwidth, KmdSysman::Requests::Memory::CurrentBandwidthRead, KmdSysman::Requests::Memory::CurrentBandwidthWrite};
        for (auto it = requestId.begin(); it != requestId.end(); it++) {
            pKmdSysManager->mockMemoryFailure[*it] = 1;
            EXPECT_EQ(ZE_RESULT_SUCCESS, zesMemoryGetBandwidth(handle, &bandwidth));
            pKmdSysManager->mockMemoryFailure[*it] = 0;
        }
    }
}

HWTEST2_F(SysmanProductHelperMemoryFixture, GivenValidMemoryHandleWhenGettingBandwidthCoverningNegativePathsThenCallFails, IsAtMostDg2) {
    auto handles = getMemoryHandles(memoryHandleComponentCount);

    for (auto handle : handles) {
        zes_mem_bandwidth_t bandwidth;

        pKmdSysManager->mockRequestMultiple = true;
        EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesMemoryGetBandwidth(handle, &bandwidth));
        pKmdSysManager->mockRequestMultiple = false;
    }
}

} // namespace ult
} // namespace Sysman
} // namespace L0
