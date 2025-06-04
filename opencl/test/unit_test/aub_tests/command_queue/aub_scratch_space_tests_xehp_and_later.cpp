/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_kernel_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"
#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"

using namespace NEO;

struct Gen12AubScratchSpaceForPrivateFixture : public KernelAUBFixture<SimpleKernelFixture> {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();

        kernelIdx = 6;
        kernelIds |= (1 << kernelIdx);
        KernelAUBFixture<SimpleKernelFixture>::setUp();

        arraySize = 32;
        vectorSize = 2;
        typeSize = sizeof(uint32_t);

        gwsSize = arraySize;
        lwsSize = 32;
        maxIterations1 = static_cast<uint32_t>(arraySize);
        maxIterations2 = static_cast<uint32_t>(arraySize);
        scalar = 0x4;

        expectedMemorySize = arraySize * vectorSize * typeSize;

        srcBuffer = alignedMalloc(expectedMemorySize, 0x1000);
        ASSERT_NE(nullptr, srcBuffer);
        auto srcBufferUint = static_cast<uint32_t *>(srcBuffer);
        uint32_t valOdd = 0x1;
        uint32_t valEven = 0x3;
        for (uint32_t i = 0; i < arraySize * vectorSize; ++i) {
            if (i % 2) {
                srcBufferUint[i] = valOdd;
            } else {
                srcBufferUint[i] = valEven;
            }
        }
        uint32_t sumOdd = 0;
        uint32_t sumEven = 0;
        for (uint32_t i = 0; i < arraySize; ++i) {
            sumOdd += ((i + scalar) + valOdd);
            sumEven += (i + valEven);
        }

        dstBuffer = clHostMemAllocINTEL(this->context, nullptr, expectedMemorySize, 0, &retVal);
        ASSERT_NE(nullptr, dstBuffer);
        memset(dstBuffer, 0, expectedMemorySize);

        expectedMemory = alignedMalloc(expectedMemorySize, 0x1000);
        ASSERT_NE(nullptr, expectedMemory);
        auto expectedMemoryUint = static_cast<uint32_t *>(expectedMemory);
        for (uint32_t i = 0; i < arraySize * vectorSize; ++i) {
            if (i % 2) {
                expectedMemoryUint[i] = sumOdd;
            } else {
                expectedMemoryUint[i] = sumEven;
            }
        }

        const auto svmManager = context->getSVMAllocsManager();
        auto svmData = svmManager->getSVMAlloc(dstBuffer);
        auto svmAllocs = &svmData->gpuAllocations;
        auto allocId = svmData->getAllocId();

        kernels[kernelIdx]->setArgSvmAlloc(0, dstBuffer, svmAllocs->getDefaultGraphicsAllocation(), allocId);
        kernels[kernelIdx]->setArgSvm(1, expectedMemorySize, srcBuffer, nullptr, 0u);
        srcAllocation = createHostPtrAllocationFromSvmPtr(srcBuffer, expectedMemorySize);

        kernels[kernelIdx]->setArg(2, sizeof(uint32_t), &scalar);
        kernels[kernelIdx]->setArg(3, sizeof(uint32_t), &maxIterations1);
        kernels[kernelIdx]->setArg(4, sizeof(uint32_t), &maxIterations2);
    }

    void tearDown() {
        pCmdQ->flush();

        if (expectedMemory) {
            alignedFree(expectedMemory);
            expectedMemory = nullptr;
        }
        if (srcBuffer) {
            alignedFree(srcBuffer);
            srcBuffer = nullptr;
        }
        if (dstBuffer) {
            clMemFreeINTEL(this->context, dstBuffer);
            dstBuffer = nullptr;
        }

        KernelAUBFixture<SimpleKernelFixture>::tearDown();
    }

    std::unique_ptr<DebugManagerStateRestore> debugRestorer;

    size_t arraySize;
    size_t vectorSize;
    size_t typeSize;
    size_t gwsSize;
    size_t lwsSize;
    uint32_t kernelIdx;

    void *expectedMemory = nullptr;
    size_t expectedMemorySize = 0;

    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    GraphicsAllocation *srcAllocation;
    GraphicsAllocation *dstAllocation;

    uint32_t scalar;
    uint32_t maxIterations1;
    uint32_t maxIterations2;
};

using Gen12AubScratchSpaceForPrivateTest = Test<Gen12AubScratchSpaceForPrivateFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, Gen12AubScratchSpaceForPrivateTest, WhenKernelUsesScratchSpaceForPrivateThenExpectCorrectResults) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {gwsSize, 1, 1};
    size_t localWorkSize[3] = {lwsSize, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto retVal = pCmdQ->enqueueKernel(
        kernels[kernelIdx].get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    expectMemory<FamilyType>(dstBuffer, expectedMemory, expectedMemorySize);
}

class DefaultGrfKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();

        std::string programName("simple_spill_fill_kernel");
        createProgramFromBinary(
            context,
            context->getDevices(),
            programName);
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel.reset(Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel("spill_test"),
            *device,
            retVal));
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void tearDown() {
        if (kernel) {
            kernel.reset(nullptr);
        }

        ProgramFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<Kernel> kernel;
};

struct Gen12AubScratchSpaceForSpillFillFixture : public KernelAUBFixture<DefaultGrfKernelFixture> {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();

        KernelAUBFixture<DefaultGrfKernelFixture>::setUp();

        arraySize = 32;
        typeSize = sizeof(cl_int);

        gwsSize = arraySize;
        lwsSize = 32;

        expectedMemorySize = (arraySize * 2 + 1) * typeSize - 4;
        inMemorySize = expectedMemorySize;
        outMemorySize = expectedMemorySize;
        offsetMemorySize = 128 * arraySize;

        srcBuffer = alignedMalloc(inMemorySize, 0x1000);
        ASSERT_NE(nullptr, srcBuffer);
        memset(srcBuffer, 0, inMemorySize);

        outBuffer = alignedMalloc(outMemorySize, 0x1000);
        ASSERT_NE(nullptr, outBuffer);
        memset(outBuffer, 0, outMemorySize);

        expectedMemory = alignedMalloc(expectedMemorySize, 0x1000);
        ASSERT_NE(nullptr, expectedMemory);
        memset(expectedMemory, 0, expectedMemorySize);

        offsetBuffer = alignedMalloc(offsetMemorySize, 0x1000);
        ASSERT_NE(nullptr, expectedMemory);
        memset(offsetBuffer, 0, offsetMemorySize);

        auto srcBufferInt = static_cast<cl_int *>(srcBuffer);
        auto expectedMemoryInt = static_cast<cl_int *>(expectedMemory);
        const int expectedVal1 = 16256;
        const int expectedVal2 = 512;

        for (uint32_t i = 0; i < arraySize; ++i) {
            srcBufferInt[i] = 2;
            expectedMemoryInt[i * 2] = expectedVal1;
            expectedMemoryInt[i * 2 + 1] = expectedVal2;
        }

        auto &kernelInfo = kernel->getKernelInfo();
        EXPECT_NE(0u, kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0]);
        EXPECT_EQ(128u, kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired);

        kernel->setArgSvm(0, inMemorySize, srcBuffer, nullptr, 0u);
        inAllocation = createHostPtrAllocationFromSvmPtr(srcBuffer, inMemorySize);

        kernel->setArgSvm(1, outMemorySize, outBuffer, nullptr, 0u);
        outAllocation = createHostPtrAllocationFromSvmPtr(outBuffer, outMemorySize);

        kernel->setArgSvm(2, offsetMemorySize, offsetBuffer, nullptr, 0u);
        offsetAllocation = createHostPtrAllocationFromSvmPtr(offsetBuffer, offsetMemorySize);
    }

    void tearDown() {
        pCmdQ->flush();

        if (expectedMemory) {
            alignedFree(expectedMemory);
            expectedMemory = nullptr;
        }
        if (srcBuffer) {
            alignedFree(srcBuffer);
            srcBuffer = nullptr;
        }
        if (outBuffer) {
            alignedFree(outBuffer);
            outBuffer = nullptr;
        }
        if (offsetBuffer) {
            alignedFree(offsetBuffer);
            offsetBuffer = nullptr;
        }

        KernelAUBFixture<DefaultGrfKernelFixture>::tearDown();
    }

    std::unique_ptr<DebugManagerStateRestore> debugRestorer;

    size_t arraySize;
    size_t vectorSize;
    size_t typeSize;
    size_t gwsSize;
    size_t lwsSize;

    void *expectedMemory = nullptr;
    size_t expectedMemorySize = 0;
    size_t inMemorySize = 0;
    size_t outMemorySize = 0;
    size_t offsetMemorySize = 0;

    void *srcBuffer = nullptr;
    void *outBuffer = nullptr;
    void *offsetBuffer = nullptr;
    GraphicsAllocation *inAllocation;
    GraphicsAllocation *outAllocation;
    GraphicsAllocation *offsetAllocation;
};

using Gen12AubScratchSpaceForSpillFillTest = Test<Gen12AubScratchSpaceForSpillFillFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, Gen12AubScratchSpaceForSpillFillTest, givenSurfaceStateScratchSpaceEnabledWhenKernelUsesScratchForSpillFillThenExpectCorrectResults) {
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {gwsSize, 1, 1};
    size_t localWorkSize[3] = {lwsSize, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto retVal = pCmdQ->enqueueKernel(
        kernel.get(),
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    expectMemory<FamilyType>(outBuffer, expectedMemory, expectedMemorySize);
}
