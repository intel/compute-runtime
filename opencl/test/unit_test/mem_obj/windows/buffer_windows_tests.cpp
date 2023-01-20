/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/raii_hw_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_hw_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/sharings/unified/unified_sharing_types.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_execution_environment.h"
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

using BufferCreateWindowsTests = testing::Test;

HWTEST_F(BufferCreateWindowsTests, givenClMemCopyHostPointerPassedToBufferCreateWhenCpuCopyAllowedThenLockResourceAndWriteBufferCorrectlyCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get());
    auto memoryManager = new MockMemoryManager(true, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    MockClDevice device(new MockDevice(executionEnvironment, mockRootDeviceIndex));
    ASSERT_TRUE(device.createEngines());
    DeviceFactory::prepareDeviceEnvironments(*device.getExecutionEnvironment());
    MockContext context(&device, true);
    auto commandQueue = new MockCommandQueue(context);
    context.setSpecialQueue(commandQueue, mockRootDeviceIndex);
    constexpr size_t smallBufferSize = Buffer::maxBufferSizeForCopyOnCpu;
    char memory[smallBufferSize];
    RAIIGfxCoreHelperFactory<MockGfxCoreHelperHwWithSetIsLockable<FamilyType>> overrideGfxCoreHelperHw{defaultHwInfo->platform.eRenderCoreFamily};

    {
        // cpu copy allowed
        cl_int retVal;
        cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
        auto writeBufferCounter = commandQueue->writeBufferCounter;
        size_t lockResourceCalled = memoryManager->lockResourceCalled;

        std::unique_ptr<Buffer> buffer(Buffer::create(&context, flags, sizeof(memory), memory, retVal));
        ASSERT_NE(nullptr, buffer.get());
        EXPECT_EQ(commandQueue->writeBufferCounter, writeBufferCounter);
        EXPECT_EQ(memoryManager->lockResourceCalled, lockResourceCalled + 1);

        writeBufferCounter = commandQueue->writeBufferCounter;
        lockResourceCalled = memoryManager->lockResourceCalled;
        overrideGfxCoreHelperHw.mockGfxCoreHelper.setIsLockable = false;

        std::unique_ptr<Buffer> bufferWhenLockNotAllowed(Buffer::create(&context, flags, sizeof(memory), memory, retVal));
        ASSERT_NE(nullptr, bufferWhenLockNotAllowed.get());
        EXPECT_EQ(commandQueue->writeBufferCounter, writeBufferCounter + 1);
        EXPECT_EQ(memoryManager->lockResourceCalled, lockResourceCalled);
    }
}

HWTEST_F(BufferCreateWindowsTests, givenClMemCopyHostPointerPassedToBufferCreateWhenCpuCopyDisAllowedThenLockResourceAndWriteBufferCorrectlyCalled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get());
    auto memoryManager = new MockMemoryManager(true, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    MockClDevice device(new MockDevice(executionEnvironment, mockRootDeviceIndex));
    ASSERT_TRUE(device.createEngines());
    DeviceFactory::prepareDeviceEnvironments(*device.getExecutionEnvironment());
    MockContext context(&device, true);
    auto commandQueue = new MockCommandQueue(context);
    context.setSpecialQueue(commandQueue, mockRootDeviceIndex);
    constexpr size_t bigBufferSize = Buffer::maxBufferSizeForCopyOnCpu + 1;
    char bigMemory[bigBufferSize];
    RAIIGfxCoreHelperFactory<MockGfxCoreHelperHwWithSetIsLockable<FamilyType>> overrideGfxCoreHelperHw{defaultHwInfo->platform.eRenderCoreFamily};

    {
        // buffer size over threshold -> cpu copy disallowed
        cl_int retVal;
        cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
        auto writeBufferCounter = commandQueue->writeBufferCounter;
        size_t lockResourceCalled = memoryManager->lockResourceCalled;

        std::unique_ptr<Buffer> buffer(Buffer::create(&context, flags, sizeof(bigMemory), bigMemory, retVal));
        ASSERT_NE(nullptr, buffer.get());
        EXPECT_EQ(commandQueue->writeBufferCounter, writeBufferCounter + 1);
        EXPECT_EQ(memoryManager->lockResourceCalled, lockResourceCalled);

        writeBufferCounter = commandQueue->writeBufferCounter;
        lockResourceCalled = memoryManager->lockResourceCalled;
        overrideGfxCoreHelperHw.mockGfxCoreHelper.setIsLockable = false;

        std::unique_ptr<Buffer> bufferWhenLockNotAllowed(Buffer::create(&context, flags, sizeof(bigMemory), bigMemory, retVal));
        ASSERT_NE(nullptr, bufferWhenLockNotAllowed.get());
        EXPECT_EQ(commandQueue->writeBufferCounter, writeBufferCounter + 1);
        EXPECT_EQ(memoryManager->lockResourceCalled, lockResourceCalled);
    }
}