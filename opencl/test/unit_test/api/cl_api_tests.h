/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_limits.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include <memory>

namespace NEO {
class CommandQueue;

template <uint32_t rootDeviceIndex = 1u>
struct ApiFixture {

    void setUp() {
        USE_REAL_FILE_SYSTEM();
        debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
        debugManager.flags.EnableCpuCacheForResources.set(true);
        debugManager.flags.ContextGroupSize.set(0);
        executionEnvironment = new ClExecutionEnvironment();
        prepareDeviceEnvironments(*executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        auto rootDevice = MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex);
        if (rootDeviceIndex != 0u) {
            rootDeviceEnvironmentBackup.swap(executionEnvironment->rootDeviceEnvironments[0]);
        }

        pDevice = new MockClDevice(rootDevice);
        ASSERT_NE(nullptr, pDevice);

        testedClDevice = pDevice;
        pContext = Context::create<MockContext>(nullptr, ClDeviceVector(&testedClDevice, 1), nullptr, nullptr, retVal);
        EXPECT_EQ(retVal, CL_SUCCESS);

        pCommandQueue = new MockCommandQueue(pContext, pDevice, nullptr, false);

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pDevice));

        pMultiDeviceKernel = MockMultiDeviceKernel::create<MockKernel>(pProgram, MockKernel::toKernelInfoContainer(pProgram->mockKernelInfo, testedRootDeviceIndex));
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(testedRootDeviceIndex));
        ASSERT_NE(nullptr, pKernel);
    }

    void tearDown() {
        pMultiDeviceKernel->release();
        pCommandQueue->release();
        pContext->release();
        pProgram->release();
        if (rootDeviceIndex != 0u) {
            rootDeviceEnvironmentBackup.swap(executionEnvironment->rootDeviceEnvironments[0]);
        }
        pDevice->decRefInternal();
    }

    void disableQueueCapabilities(cl_command_queue_capabilities_intel capabilities) {
        if (pCommandQueue->queueCapabilities == CL_QUEUE_DEFAULT_CAPABILITIES_INTEL) {
            pCommandQueue->queueCapabilities = pDevice->getQueueFamilyCapabilitiesAll();
        }

        pCommandQueue->queueCapabilities &= ~capabilities;
    }

    DebugManagerStateRestore restorer;
    cl_int retVal = CL_SUCCESS;
    size_t retSize = 0;

    MockCommandQueue *pCommandQueue = nullptr;
    Context *pContext = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    MockKernel *pKernel = nullptr;
    MockProgram *pProgram = nullptr;
    constexpr static uint32_t numRootDevices = maxRootDeviceCount;
    constexpr static uint32_t testedRootDeviceIndex = rootDeviceIndex;
    cl_device_id testedClDevice = nullptr;
    MockClDevice *pDevice = nullptr;
    ClExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<RootDeviceEnvironment> rootDeviceEnvironmentBackup;
    inline static const char *sampleKernel = "example_kernel(){}";
    inline static size_t sampleKernelSize = std::strlen(sampleKernel) + 1;
    inline static const char *sampleKernelSrcs[1] = {sampleKernel};
};

struct ApiTests : public ApiFixture<>,
                  public ::testing::Test {
    void SetUp() override {
        ApiFixture::setUp();
    }
    void TearDown() override {
        ApiFixture::tearDown();
    }
};

struct ApiFixtureUsingAlignedMemoryManager {
  public:
    void setUp();
    void tearDown();

    cl_int retVal;
    size_t retSize;

    CommandQueue *commandQueue;
    Context *context;
    MockKernel *kernel;
    MockProgram *program;
    MockClDevice *device;
};

using api_test_using_aligned_memory_manager = Test<ApiFixtureUsingAlignedMemoryManager>;

void CL_CALLBACK notifyFuncProgram(
    cl_program program,
    void *userData);
} // namespace NEO
