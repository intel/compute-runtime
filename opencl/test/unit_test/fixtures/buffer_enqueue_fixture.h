/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/helpers/cl_execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

struct BufferEnqueueFixture : public ClHardwareParse,
                              public ::testing::Test {
    BufferEnqueueFixture(void)
        : buffer(nullptr) {
    }

    void SetUp() override {
        executionEnvironment = getClExecutionEnvironmentImpl(hwInfo, 1);
    }

    void TearDown() override {
        buffer.reset(nullptr);
    }

    template <typename FamilyType>
    void initializeFixture() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsrHw2<FamilyType>>();

        memoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

        context = std::make_unique<MockContext>(device.get());

        bufferMemory = std::make_unique<uint32_t[]>(alignUp(bufferSizeInDwords, sizeof(uint32_t)));
        cl_int retVal = 0;

        buffer.reset(Buffer::create(context.get(),
                                    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                    bufferSizeInDwords,
                                    reinterpret_cast<char *>(bufferMemory.get()),
                                    retVal));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }

  protected:
    const size_t bufferSizeInDwords = 64;
    HardwareInfo hardwareInfo;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment;
    cl_queue_properties properties = {};
    std::unique_ptr<uint32_t[]> bufferMemory;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;

    MockMemoryManager *memoryManager = nullptr;
};

struct EnqueueReadWriteBufferRectDispatch : public BufferEnqueueFixture {

    void SetUp() override {
        BufferEnqueueFixture::SetUp();
    }

    void TearDown() override {
        BufferEnqueueFixture::TearDown();
    }

    uint32_t memory[64] = {0};

    size_t bufferOrigin[3] = {0, 0, 0};
    size_t hostOrigin[3] = {1, 1, 1};
    size_t region[3] = {1, 2, 1};

    size_t bufferRowPitch = 4;
    size_t bufferSlicePitch = bufferSizeInDwords;

    size_t hostRowPitch = 5;
    size_t hostSlicePitch = 15;
};
