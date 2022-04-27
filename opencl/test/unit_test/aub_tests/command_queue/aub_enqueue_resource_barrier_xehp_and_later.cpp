/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/cmd_buffer_validator.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "test_traits_common.h"

using namespace NEO;

using ResourceBarrierAubTest = Test<KernelAUBFixture<SimpleKernelFixture>>;

struct L3ControlSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::l3ControlSupported;
        }
        return false;
    }
};

HWTEST2_F(ResourceBarrierAubTest, givenAllocationsWhenEnqueueResourceBarrierCalledThenL3FlushCommandWasSubmitted, L3ControlSupportedMatcher) {
    using L3_CONTROL = typename FamilyType::L3_CONTROL;

    constexpr size_t bufferSize = MemoryConstants::pageSize;
    char bufferAMemory[bufferSize];
    char bufferBMemory[bufferSize];

    memset(bufferAMemory, 1, bufferSize);
    memset(bufferBMemory, 129, bufferSize);

    auto retVal = CL_INVALID_VALUE;
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(context,
                                                            CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                            bufferSize, bufferAMemory, retVal));

    ASSERT_NE(nullptr, srcBuffer);
    auto dstBuffer1 = std::unique_ptr<Buffer>(Buffer::create(context,
                                                             CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                             bufferSize, bufferBMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer1);

    auto dstBuffer2 = std::unique_ptr<Buffer>(Buffer::create(context,
                                                             CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                             bufferSize, bufferBMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer2);

    retVal = pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer1.get(),
                                      0, 0,
                                      bufferSize, 0,
                                      nullptr, nullptr);

    retVal = pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer2.get(),
                                      0, 0,
                                      bufferSize, 0,
                                      nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_resource_barrier_descriptor_intel descriptor{};
    cl_resource_barrier_descriptor_intel descriptor2{};

    descriptor.mem_object = dstBuffer1.get();
    descriptor2.mem_object = dstBuffer2.get();

    const cl_resource_barrier_descriptor_intel descriptors[] = {descriptor, descriptor2};

    BarrierCommand bCmd(pCmdQ, descriptors, 2);

    auto sizeUsed = pCmdQ->getCS(0).getUsed();

    retVal = pCmdQ->enqueueResourceBarrier(&bCmd, 0, nullptr, nullptr);

    LinearStream &l3FlushCmdStream = pCmdQ->getCS(0);

    std::string err;
    auto cmdBuffOk = expectCmdBuff<FamilyType>(l3FlushCmdStream, sizeUsed,
                                               std::vector<MatchCmd *>{
                                                   new MatchAnyCmd(AnyNumber),
                                                   new MatchHwCmd<FamilyType, L3_CONTROL>(1),
                                                   new MatchAnyCmd(AnyNumber),
                                               },
                                               &err);
    EXPECT_TRUE(cmdBuffOk) << err;

    retVal = pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer2.get(),
                                      0, 0,
                                      bufferSize, 0,
                                      nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    expectMemory<FamilyType>(reinterpret_cast<void *>(dstBuffer1->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress()),
                             bufferAMemory, bufferSize);
    expectMemory<FamilyType>(reinterpret_cast<void *>(dstBuffer2->getGraphicsAllocation(device->getRootDeviceIndex())->getGpuAddress()),
                             bufferAMemory, bufferSize);
}
