/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

static const unsigned int g_scTestBufferSizeInBytes = 16;

TEST(Buffer, WhenValidateHandleTypeThenReturnFalse) {
    MemoryProperties memoryProperties;
    UnifiedSharingMemoryDescription extMem;
    bool isValid = Buffer::validateHandleType(memoryProperties, extMem);
    EXPECT_FALSE(isValid);
}

class ExportBufferTests : public ClDeviceFixture,
                          public testing::Test {
  public:
    ExportBufferTests() {
    }

  protected:
    void SetUp() override {
        flags = CL_MEM_READ_WRITE;
        ClDeviceFixture::setUp();
        context.reset(new MockContext(pClDevice));
    }

    void TearDown() override {
        context.reset();
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    std::unique_ptr<MockContext> context;
    MemoryManager *contextMemoryManager;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    unsigned char pHostPtr[g_scTestBufferSizeInBytes];
};

struct ValidExportHostPtr
    : public ExportBufferTests,
      public MemoryManagementFixture {
    typedef ExportBufferTests BaseClass;

    using ExportBufferTests::SetUp;
    using MemoryManagementFixture::setUp;

    ValidExportHostPtr() {
    }

    void SetUp() override {
        MemoryManagementFixture::setUp();
        BaseClass::SetUp();

        ASSERT_NE(nullptr, pDevice);
    }

    void TearDown() override {
        delete buffer;
        BaseClass::TearDown();
        MemoryManagementFixture::tearDown();
    }

    Buffer *createBuffer() {
        return Buffer::create(
            context.get(),
            flags,
            g_scTestBufferSizeInBytes,
            pHostPtr,
            retVal);
    }

    cl_int retVal = CL_INVALID_VALUE;
    Buffer *buffer = nullptr;
};

TEST_F(ValidExportHostPtr, givenPropertiesWithDmaBufWhenValidateInputAndCreateBufferThenInvalidPropertyIsSet) {
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 0x1234, 0};
    auto buffer = BufferFunctions::validateInputAndCreateBuffer(context.get(), properties, flags, 0, g_scTestBufferSizeInBytes, nullptr, retVal);
    EXPECT_EQ(retVal, CL_INVALID_PROPERTY);
    EXPECT_EQ(nullptr, buffer);

    clReleaseMemObject(buffer);
}

TEST_F(ValidExportHostPtr, givenInvalidPropertiesWithNtHandleWhenValidateInputAndCreateBufferThenCorrectBufferIsSet) {

    osHandle invalidHandle = static_cast<MockMemoryManager *>(pClExecutionEnvironment->memoryManager.get())->invalidSharedHandle;
    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, invalidHandle, 0};
    cl_mem buffer = BufferFunctions::validateInputAndCreateBuffer(context.get(), properties, flags, 0, g_scTestBufferSizeInBytes, nullptr, retVal);

    EXPECT_EQ(retVal, CL_INVALID_MEM_OBJECT);
    EXPECT_EQ(static_cast<MockMemoryManager *>(pClExecutionEnvironment->memoryManager.get())->capturedSharedHandle, properties[1]);
    EXPECT_EQ(buffer, nullptr);

    clReleaseMemObject(buffer);
}

TEST_F(ValidExportHostPtr, givenPropertiesWithNtHandleWhenValidateInputAndCreateBufferThenCorrectBufferIsSet) {

    cl_mem_properties properties[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x1234, 0};
    cl_mem buffer = BufferFunctions::validateInputAndCreateBuffer(context.get(), properties, flags, 0, g_scTestBufferSizeInBytes, nullptr, retVal);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(static_cast<MockMemoryManager *>(pClExecutionEnvironment->memoryManager.get())->capturedSharedHandle, properties[1]);
    EXPECT_EQ(static_cast<Buffer *>(buffer)->getGraphicsAllocation(0u)->getAllocationType(), NEO::AllocationType::SHARED_BUFFER);
    EXPECT_NE(buffer, nullptr);

    clReleaseMemObject(buffer);
}
