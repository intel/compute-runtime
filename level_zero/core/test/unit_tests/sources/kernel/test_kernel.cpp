/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_device.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using KernelImpSetGroupSizeTest = Test<DeviceFixture>;

HWTEST_F(KernelImpSetGroupSizeTest, WhenCalculatingLocalIdsThenGrfSizeIsTakenFromCapabilityTable) {
    Mock<Kernel> mockKernel;
    Mock<Module> mockModule(this->device, this->neoDevice, nullptr);
    mockKernel.descriptor.kernelAttributes.simdSize = 1;
    mockKernel.module = &mockModule;
    auto grfSize = mockModule.getDevice()->getHwInfo().capabilityTable.grfSize;
    uint32_t groupSize[3] = {2, 3, 5};
    auto ret = mockKernel.setGroupSize(groupSize[0], groupSize[1], groupSize[2]);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(groupSize[0] * groupSize[1] * groupSize[2], mockKernel.numThreadsPerThreadGroup);
    EXPECT_EQ(grfSize * groupSize[0] * groupSize[1] * groupSize[2], mockKernel.perThreadDataSizeForWholeThreadGroup);
    ASSERT_LE(grfSize * groupSize[0] * groupSize[1] * groupSize[2], mockKernel.perThreadDataSizeForWholeThreadGroup);
    using LocalIdT = unsigned short;
    auto threadOffsetInLocalIds = grfSize / sizeof(LocalIdT);
    auto generatedLocalIds = reinterpret_cast<LocalIdT *>(mockKernel.perThreadDataForWholeThreadGroup);

    uint32_t threadId = 0;
    for (uint32_t z = 0; z < groupSize[2]; ++z) {
        for (uint32_t y = 0; y < groupSize[1]; ++y) {
            for (uint32_t x = 0; x < groupSize[0]; ++x) {
                EXPECT_EQ(x, generatedLocalIds[0 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                EXPECT_EQ(y, generatedLocalIds[1 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                EXPECT_EQ(z, generatedLocalIds[2 + threadId * threadOffsetInLocalIds]) << " thread : " << threadId;
                ++threadId;
            }
        }
    }
}

} // namespace ult
} // namespace L0
