/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/execution_model_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include <memory>

using namespace NEO;

class MockKernelWithArgumentAccess : public Kernel {
  public:
    std::vector<SimpleKernelArgInfo> &getKernelArguments() {
        return kernelArguments;
    }

    class ObjectCountsPublic : public Kernel::ObjectCounts {
    };

    MockKernelWithArgumentAccess(Program *programArg, KernelInfo &kernelInfoArg, ClDevice &clDeviceArg) : Kernel(programArg, kernelInfoArg, clDeviceArg, false) {
    }

    void getParentObjectCountsPublic(MockKernelWithArgumentAccess::ObjectCountsPublic &objectCount) {
        getParentObjectCounts(objectCount);
    }
};

TEST(ParentKernelTest, WhenArgsAddedThenObjectCountsAreIncremented) {
    MockClDevice *device = new MockClDevice{new MockDevice};
    MockProgram program(toClDeviceVector(*device));
    KernelInfo info;
    info.kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;

    MockKernelWithArgumentAccess kernel(&program, info, *device);

    std::vector<Kernel::SimpleKernelArgInfo> &args = kernel.getKernelArguments();

    Kernel::SimpleKernelArgInfo argInfo;
    argInfo.type = Kernel::kernelArgType::SAMPLER_OBJ;
    args.push_back(argInfo);
    argInfo.type = Kernel::kernelArgType::IMAGE_OBJ;
    args.push_back(argInfo);

    MockKernelWithArgumentAccess::ObjectCountsPublic objectCounts;

    kernel.getParentObjectCountsPublic(objectCounts);

    EXPECT_EQ(1u, objectCounts.imageCount);
    EXPECT_EQ(1u, objectCounts.samplerCount);

    delete device;
}

TEST(ParentKernelTest, WhenPatchingBlocksSimdSizeThenPatchIsAppliedCorrectly) {
    MockClDevice device{new MockDevice};
    MockContext context(&device);
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, true));
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    parentKernel->patchBlocksSimdSize();

    void *blockSimdSize = ptrOffset(parentKernel->getCrossThreadData(), parentKernel->getKernelInfo().childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    EXPECT_EQ(program->blockKernelManager->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);
}

TEST(ParentKernelTest, GivenParentKernelWhenCheckingForDeviceEnqueueThenTrueIsReturned) {
    MockClDevice device{new MockDevice};
    MockContext context(&device);
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context));

    EXPECT_TRUE(parentKernel->getKernelInfo().hasDeviceEnqueue());
}

TEST(ParentKernelTest, GivenNormalKernelWhenCheckingForDeviceEnqueueThenFalseIsReturned) {
    MockClDevice device{new MockDevice};
    MockKernelWithInternals kernel(device);

    EXPECT_FALSE(kernel.kernelInfo.hasDeviceEnqueue());
}

TEST(ParentKernelTest, WhenInitializingParentKernelThenBlocksSimdSizeIsPatched) {
    MockClDevice device{new MockDevice};
    MockContext context(&device);
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, true));
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    parentKernel->initialize();

    void *blockSimdSize = ptrOffset(parentKernel->getCrossThreadData(), parentKernel->getKernelInfo().childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    EXPECT_EQ(program->blockKernelManager->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);
}

TEST(ParentKernelTest, WhenInitializingParentKernelThenPrivateMemoryForBlocksIsAllocated) {
    MockClDevice device{new MockDevice};
    MockContext context(&device);
    std::unique_ptr<MockParentKernel> parentKernel(MockParentKernel::create(context, true));
    MockProgram *program = (MockProgram *)parentKernel->mockProgram;

    auto infoBlock = new MockKernelInfo();
    infoBlock->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    uint32_t crossThreadOffsetBlock = 0;
    infoBlock->setDeviceSideEnqueueDefaultQueueSurface(8, crossThreadOffsetBlock);
    crossThreadOffsetBlock += 8;

    infoBlock->setDeviceSideEnqueueEventPoolSurface(8, crossThreadOffsetBlock);
    crossThreadOffsetBlock += 8;

    infoBlock->setPrivateMemory(1000, false, 8, crossThreadOffsetBlock);
    crossThreadOffsetBlock += 8;

    infoBlock->setLocalIds({0, 0, 0});

    infoBlock->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
    infoBlock->setDeviceSideEnqueueBlockInterfaceDescriptorOffset(0);

    infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
    infoBlock->heapInfo.DynamicStateHeapSize = 64 * sizeof(uint64_t);

    infoBlock->setCrossThreadDataSize(crossThreadOffsetBlock);
    infoBlock->crossThreadData = new char[crossThreadOffsetBlock];

    program->blockKernelManager->addBlockKernelInfo(infoBlock);

    parentKernel->initialize();

    EXPECT_NE(nullptr, program->getBlockKernelManager()->getPrivateSurface(program->getBlockKernelManager()->getCount() - 1));
}

struct ParentKernelFromBinaryTest : public ExecutionModelKernelFixture {

    void SetUp() override {
        ExecutionModelKernelFixture::SetUp("simple_block_kernel", "simple_block_kernel");
    }
};

TEST_F(ParentKernelFromBinaryTest, GivenParentKernelWhenGettingInstructionHeapSizeForExecutionModelThenSizeIsGreaterThanZero) {
    EXPECT_TRUE(pKernel->isParentKernel);
    EXPECT_LT(0u, pKernel->getInstructionHeapSizeForExecutionModel());
}
