/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/aligned_memory.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/context/context.h"
#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "gtest/gtest.h"

using namespace NEO;

const int maxHintCounter = 6;

bool containsHint(const char *providedHint, char *userData);

void CL_CALLBACK callbackFunction(const char *providedHint, const void *flags, size_t size, void *userData);

struct DriverDiagnosticsTest : public PlatformFixture,
                               public ::testing::Test {
    using PlatformFixture::SetUp;
    DriverDiagnosticsTest() : retVal(CL_SUCCESS) {
    }

    void SetUp() override {
        PlatformFixture::SetUp();
        memset(userData, 0, maxHintCounter * DriverDiagnostics::maxHintStringSize);
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }

    cl_int retVal;
    char userData[maxHintCounter * DriverDiagnostics::maxHintStringSize];
    char expectedHint[DriverDiagnostics::maxHintStringSize];
};

struct VerboseLevelTest : public DriverDiagnosticsTest,
                          public ::testing::WithParamInterface<unsigned int> {

    void SetUp() override {
        DriverDiagnosticsTest::SetUp();
        hintId = CL_BUFFER_NEEDS_ALLOCATE_MEMORY;
        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[hintId], 0);
    }

    void TearDown() override {
        DriverDiagnosticsTest::TearDown();
    }
    std::vector<unsigned int> validLevels{
        CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL,
        CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL,
        CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL,
        CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL};
    PerformanceHints hintId;
};

struct PerformanceHintTest : public DriverDiagnosticsTest,
                             public CommandQueueHwFixture {

    void SetUp() override {
        DriverDiagnosticsTest::SetUp();
        cl_device_id deviceID = devices[0];
        cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
        context = Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        CommandQueueHwFixture::TearDown();
        DriverDiagnosticsTest::TearDown();
    }
};

struct PerformanceHintBufferTest : public PerformanceHintTest,
                                   public ::testing::WithParamInterface<std::tuple<bool /*address aligned*/, bool /*size aligned*/>> {

    void SetUp() override {
        PerformanceHintTest::SetUp();
        std::tie(alignedAddress, alignedSize) = GetParam();
        address = alignedMalloc(2 * MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    }

    void TearDown() override {
        delete buffer;
        alignedFree(address);
        PerformanceHintTest::TearDown();
    }
    bool alignedSize;
    bool alignedAddress;
    void *address;
    Buffer *buffer;
};

struct PerformanceHintCommandQueueTest : public PerformanceHintTest,
                                         public ::testing::WithParamInterface<std::tuple<bool /*profiling enabled*/, bool /*preemption supported*/>> {

    void SetUp() override {
        PerformanceHintTest::SetUp();
        std::tie(profilingEnabled, preemptionSupported) = GetParam();
        device = new MockDevice;
        device->deviceInfo.preemptionSupported = preemptionSupported;
    }

    void TearDown() override {
        clReleaseCommandQueue(cmdQ);
        delete device;
        PerformanceHintTest::TearDown();
    }
    MockDevice *device;
    cl_command_queue cmdQ;
    bool profilingEnabled;
    bool preemptionSupported;
};

struct PerformanceHintEnqueueTest : public PerformanceHintTest {
    void SetUp() override {
        PerformanceHintTest::SetUp();
        pCmdQ = createCommandQueue(pPlatform->getDevice(0));
    }

    void TearDown() override {
        PerformanceHintTest::TearDown();
    }
};

struct PerformanceHintEnqueueBufferTest : public PerformanceHintEnqueueTest {
    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
        address = alignedMalloc(2 * MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
        buffer = Buffer::create(
            context,
            CL_MEM_USE_HOST_PTR,
            MemoryConstants::cacheLineSize,
            address,
            retVal);
    }

    void TearDown() override {
        delete buffer;
        alignedFree(address);
        PerformanceHintEnqueueTest::TearDown();
    }
    void *address;
    Buffer *buffer;
};

struct PerformanceHintEnqueueReadBufferTest : public PerformanceHintEnqueueBufferTest,
                                              public ::testing::WithParamInterface<std::tuple<bool /*address aligned*/, bool /*size aligned*/>> {

    void SetUp() override {
        PerformanceHintEnqueueBufferTest::SetUp();
        std::tie(alignedAddress, alignedSize) = GetParam();
    }

    void TearDown() override {
        PerformanceHintEnqueueBufferTest::TearDown();
    }
    bool alignedSize;
    bool alignedAddress;
};

struct PerformanceHintEnqueueImageTest : public PerformanceHintEnqueueTest {

    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
        address = alignedMalloc(2 * MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
        image = ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context);
        zeroCopyImage.reset(ImageHelper<Image1dDefaults>::create(context));
    }

    void TearDown() override {
        delete image;
        zeroCopyImage.reset(nullptr);
        alignedFree(address);
        PerformanceHintEnqueueTest::TearDown();
    }
    void *address;
    Image *image;
    std::unique_ptr<Image> zeroCopyImage;
};

struct PerformanceHintEnqueueReadImageTest : public PerformanceHintEnqueueImageTest,
                                             public ::testing::WithParamInterface<std::tuple<bool /*address aligned*/, bool /*size aligned*/>> {

    void SetUp() override {
        PerformanceHintEnqueueImageTest::SetUp();
        std::tie(alignedAddress, alignedSize) = GetParam();
    }

    void TearDown() override {
        PerformanceHintEnqueueImageTest::TearDown();
    }
    bool alignedSize;
    bool alignedAddress;
};

struct PerformanceHintEnqueueMapTest : public PerformanceHintEnqueueTest,
                                       public ::testing::WithParamInterface<bool /*zero-copy obj*/> {

    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
    }

    void TearDown() override {
        PerformanceHintEnqueueTest::TearDown();
    }
};

struct PerformanceHintEnqueueKernelTest : public PerformanceHintEnqueueTest,
                                          public ProgramFixture {

    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
        cl_device_id device = pPlatform->getDevice(0);
        CreateProgramFromBinary(context, &device, "CopyBuffer_simd32");
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        ASSERT_EQ(CL_SUCCESS, retVal);
        kernel = Kernel::create<MockKernel>(pProgram, *pProgram->getKernelInfo("CopyBuffer"), &retVal);

        globalWorkGroupSize[0] = globalWorkGroupSize[1] = globalWorkGroupSize[2] = 1;
    }

    void TearDown() override {
        delete kernel;
        ProgramFixture::TearDown();
        PerformanceHintEnqueueTest::TearDown();
    }
    Kernel *kernel;
    size_t globalWorkGroupSize[3];
};

struct PerformanceHintEnqueueKernelBadSizeTest : public PerformanceHintEnqueueKernelTest,
                                                 public ::testing::WithParamInterface<int /*bad size dimension*/> {

    void SetUp() override {
        PerformanceHintEnqueueKernelTest::SetUp();
        globalWorkGroupSize[0] = globalWorkGroupSize[1] = globalWorkGroupSize[2] = 32;
    }

    void TearDown() override {
        PerformanceHintEnqueueKernelTest::TearDown();
    }
};

struct PerformanceHintEnqueueKernelPrintfTest : public PerformanceHintEnqueueTest,
                                                public ProgramFixture {

    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
        cl_device_id device = pPlatform->getDevice(0);
        CreateProgramFromBinary(context, &device, "printf");
        retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
        ASSERT_EQ(CL_SUCCESS, retVal);
        kernel = Kernel::create(pProgram, *pProgram->getKernelInfo("test"), &retVal);

        globalWorkGroupSize[0] = globalWorkGroupSize[1] = globalWorkGroupSize[2] = 1;
    }

    void TearDown() override {
        delete kernel;
        ProgramFixture::TearDown();
        PerformanceHintEnqueueTest::TearDown();
    }
    Kernel *kernel;
    size_t globalWorkGroupSize[3];
};

struct PerformanceHintKernelTest : public PerformanceHintTest,
                                   public ::testing::WithParamInterface<bool /*zero-sized*/> {

    void SetUp() override {
        PerformanceHintTest::SetUp();
        zeroSized = GetParam();
    }

    void TearDown() override {
        PerformanceHintTest::TearDown();
    }
    bool zeroSized;
};
