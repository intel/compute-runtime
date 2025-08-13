/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/context/context.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

inline constexpr int maxHintCounter = 6;

bool containsHint(const char *providedHint, char *userData);

void CL_CALLBACK callbackFunction(const char *providedHint, const void *flags, size_t size, void *userData);

struct DriverDiagnosticsTest : public PlatformFixture,
                               public ::testing::Test {
    using PlatformFixture::setUp;
    void SetUp() override {
        PlatformFixture::setUp();
        memset(userData, 0, maxHintCounter * DriverDiagnostics::maxHintStringSize);
    }

    void TearDown() override {
        PlatformFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    char userData[maxHintCounter * DriverDiagnostics::maxHintStringSize]{};
    char expectedHint[DriverDiagnostics::maxHintStringSize]{};
};

struct VerboseLevelTest : public DriverDiagnosticsTest,
                          public ::testing::WithParamInterface<uint64_t> {

    void SetUp() override {
        DriverDiagnosticsTest::SetUp();
        hintId = CL_BUFFER_NEEDS_ALLOCATE_MEMORY;
        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[hintId], 0);
    }

    void TearDown() override {
        DriverDiagnosticsTest::TearDown();
    }
    std::vector<uint64_t> validLevels{
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
        cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
        context = Context::create<NEO::MockContext>(validProperties, ClDeviceVector(devices, numDevices), callbackFunction, (void *)userData, retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        CommandQueueHwFixture::tearDown();
        DriverDiagnosticsTest::TearDown();
    }
};

struct PerformanceHintBufferTest : public PerformanceHintTest,
                                   public ::testing::WithParamInterface<std::tuple<bool /*address aligned*/, bool /*size aligned*/, bool /*provide performance hint*/>> {

    void SetUp() override {
        PerformanceHintTest::SetUp();
        std::tie(alignedAddress, alignedSize, providePerformanceHint) = GetParam();
        address = alignedMalloc(2 * MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    }

    void TearDown() override {
        delete buffer;
        alignedFree(address);
        PerformanceHintTest::TearDown();
    }
    bool alignedSize = false;
    bool alignedAddress = false;
    bool providePerformanceHint = false;
    void *address = nullptr;
    Buffer *buffer = nullptr;
};

struct PerformanceHintCommandQueueTest : public PerformanceHintTest,
                                         public ::testing::WithParamInterface<std::tuple<bool /*profiling enabled*/, bool /*preemption supported*/>> {

    void SetUp() override {
        PerformanceHintTest::SetUp();
        std::tie(profilingEnabled, preemptionSupported) = GetParam();
        static_cast<MockClDevice *>(context->getDevice(0))->deviceInfo.preemptionSupported = preemptionSupported;
    }

    void TearDown() override {
        clReleaseCommandQueue(cmdQ);
        PerformanceHintTest::TearDown();
    }
    cl_command_queue cmdQ = nullptr;
    bool profilingEnabled = false;
    bool preemptionSupported = false;
};

struct PerformanceHintEnqueueTest : public PerformanceHintTest {
    void SetUp() override {
        PerformanceHintTest::SetUp();
        pCmdQ = createCommandQueue(pPlatform->getClDevice(0));
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
    void *address = nullptr;
    Buffer *buffer = nullptr;
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
    bool alignedSize = false;
    bool alignedAddress = false;
};

struct PerformanceHintEnqueueImageTest : public PerformanceHintEnqueueTest {

    void SetUp() override {
        REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

        PerformanceHintEnqueueTest::SetUp();
        address = alignedMalloc(2 * MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
        image = ImageHelperUlt<ImageUseHostPtr<Image1dDefaults>>::create(context);
        zeroCopyImage.reset(ImageHelperUlt<Image1dDefaults>::create(context));
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        delete image;
        zeroCopyImage.reset(nullptr);
        alignedFree(address);
        PerformanceHintEnqueueTest::TearDown();
    }
    void *address = nullptr;
    Image *image = nullptr;
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
    bool alignedSize = false;
    bool alignedAddress = false;
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
        createProgramFromBinary(context, context->getDevices(), "CopyBuffer_simd32");
        retVal = pProgram->build(pProgram->getDevices(), nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
        kernel = Kernel::create<MockKernel>(pProgram, pProgram->getKernelInfoForKernel("CopyBuffer"), *context->getDevice(0), retVal);

        globalWorkGroupSize[0] = globalWorkGroupSize[1] = globalWorkGroupSize[2] = 1;
        rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    }

    void TearDown() override {
        delete kernel;
        ProgramFixture::tearDown();
        PerformanceHintEnqueueTest::TearDown();
    }
    MockKernel *kernel = nullptr;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
    size_t globalWorkGroupSize[3]{};
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
    class KernelWhitebox : public Kernel {
      public:
        using Kernel::initializeLocalIdsCache;
    };

    void SetUp() override {
        PerformanceHintEnqueueTest::SetUp();
        createProgramFromBinary(context, context->getDevices(), "simple_kernels");
        retVal = pProgram->build(pProgram->getDevices(), nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
        kernel = static_cast<KernelWhitebox *>(Kernel::create(pProgram, pProgram->getKernelInfoForKernel("test_printf"), *context->getDevice(0), retVal));
        kernel->initializeLocalIdsCache();

        globalWorkGroupSize[0] = globalWorkGroupSize[1] = globalWorkGroupSize[2] = 1;
    }

    void TearDown() override {
        delete kernel;
        ProgramFixture::tearDown();
        PerformanceHintEnqueueTest::TearDown();
    }
    KernelWhitebox *kernel = nullptr;
    size_t globalWorkGroupSize[3]{};
};

struct PerformanceHintKernelTest : public PerformanceHintTest,
                                   public ::testing::WithParamInterface<bool /*zero-sized*/> {

    void SetUp() override {
        debugManager.flags.CreateMultipleRootDevices.set(2);
        debugManager.flags.EnableMultiRootDeviceContexts.set(true);
        PerformanceHintTest::SetUp();
        zeroSized = GetParam();
    }

    void TearDown() override {
        PerformanceHintTest::TearDown();
    }
    DebugManagerStateRestore restorer;
    bool zeroSized = false;
};
