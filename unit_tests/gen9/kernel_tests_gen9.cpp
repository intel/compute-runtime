/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace NEO;

using Gen9KernelTest = Test<DeviceFixture>;
GEN9TEST_F(Gen9KernelTest, givenKernelWhenCanTransformImagesIsCalledThenReturnsTrue) {
    MockKernelWithInternals mockKernel(*pDevice);
    auto retVal = mockKernel.mockKernel->Kernel::canTransformImages();
    EXPECT_TRUE(retVal);
}
using Gen9HardwareCommandsTest = testing::Test;
GEN9TEST_F(Gen9HardwareCommandsTest, givenGen9PlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::doBindingTablePrefetch());
}

GEN9TEST_F(Gen9HardwareCommandsTest, givenBufferThatIsNotZeroCopyWhenSurfaceStateisSetThenL3IsTurnedOn) {
    MockContext context;

    auto retVal = CL_SUCCESS;
    char ptr[16u] = {};

    std::unique_ptr<Buffer> buffer(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        16u,
        ptr,
        retVal));

    EXPECT_FALSE(buffer->isMemObjZeroCopy());

    using RENDER_SURFACE_STATE = typename SKLFamily::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE surfaceState = {};

    auto gmmHelper = context.getDevice(0)->getExecutionEnvironment()->getGmmHelper();
    gmmHelper->setSimplifiedMocsTableUsage(true);

    buffer->setArgStateful(&surfaceState, false, false);
    //make sure proper mocs is selected
    constexpr uint32_t expectedMocs = GmmHelper::cacheEnabledIndex;
    EXPECT_EQ(expectedMocs, surfaceState.getMemoryObjectControlStateIndexToMocsTables());
}
