/*
 * Copyright (C) 2017-2021 Intel Corporation
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

    uint32_t crossThreadOffsetBlock = 0;

    auto infoBlock = new KernelInfo();
    infoBlock->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
    SPatchAllocateStatelessDefaultDeviceQueueSurface allocateDeviceQueueSurface = {};
    allocateDeviceQueueSurface.DataParamOffset = crossThreadOffsetBlock;
    allocateDeviceQueueSurface.DataParamSize = 8;
    allocateDeviceQueueSurface.Size = 8;
    populateKernelDescriptor(infoBlock->kernelDescriptor, allocateDeviceQueueSurface);

    crossThreadOffsetBlock += 8;

    SPatchAllocateStatelessEventPoolSurface allocateEventPoolSurface = {};
    allocateEventPoolSurface.DataParamOffset = crossThreadOffsetBlock;
    allocateEventPoolSurface.DataParamSize = 8;
    allocateEventPoolSurface.EventPoolSurfaceIndex = 0;
    allocateEventPoolSurface.Size = 8;
    populateKernelDescriptor(infoBlock->kernelDescriptor, allocateEventPoolSurface);

    crossThreadOffsetBlock += 8;

    SPatchAllocateStatelessPrivateSurface privateSurfaceBlock = {};
    privateSurfaceBlock.DataParamOffset = crossThreadOffsetBlock;
    privateSurfaceBlock.DataParamSize = 8;
    privateSurfaceBlock.Size = 8;
    privateSurfaceBlock.SurfaceStateHeapOffset = 0;
    privateSurfaceBlock.Token = 0;
    privateSurfaceBlock.PerThreadPrivateMemorySize = 1000;
    populateKernelDescriptor(infoBlock->kernelDescriptor, privateSurfaceBlock);

    crossThreadOffsetBlock += 8;

    SPatchThreadPayload threadPayloadBlock = {};
    threadPayloadBlock.LocalIDXPresent = 0;
    threadPayloadBlock.LocalIDYPresent = 0;
    threadPayloadBlock.LocalIDZPresent = 0;
    threadPayloadBlock.HeaderPresent = 0;
    threadPayloadBlock.Size = 128;
    populateKernelDescriptor(infoBlock->kernelDescriptor, threadPayloadBlock);

    infoBlock->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;

    SPatchDataParameterStream streamBlock = {};
    streamBlock.DataParameterStreamSize = 0;
    streamBlock.Size = 0;
    populateKernelDescriptor(infoBlock->kernelDescriptor, streamBlock);

    SPatchBindingTableState bindingTable = {};
    bindingTable.Count = 0;
    bindingTable.Offset = 0;
    bindingTable.Size = 0;
    bindingTable.SurfaceStateOffset = 0;
    populateKernelDescriptor(infoBlock->kernelDescriptor, bindingTable);

    SPatchInterfaceDescriptorData idData = {};
    idData.BindingTableOffset = 0;
    idData.KernelOffset = 0;
    idData.Offset = 0;
    idData.SamplerStateOffset = 0;
    idData.Size = 0;
    populateKernelDescriptor(infoBlock->kernelDescriptor, idData);

    infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
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
