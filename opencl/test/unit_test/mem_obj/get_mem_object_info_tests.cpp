/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class GetMemObjectInfo : public ::testing::Test, public PlatformFixture, public ClDeviceFixture {
    using ClDeviceFixture::setUp;
    using PlatformFixture::setUp;

  public:
    void SetUp() override {
        PlatformFixture::setUp();
        ClDeviceFixture::setUp();
        BufferDefaults::context = new MockContext;
    }

    void TearDown() override {
        delete BufferDefaults::context;
        ClDeviceFixture::tearDown();
        PlatformFixture::tearDown();
    }
};

TEST_F(GetMemObjectInfo, GivenInvalidParamsWhenGettingMemObjectInfoThenInvalidValueErrorIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = buffer->getMemObjectInfo(
        0,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetMemObjectInfo, GivenInvalidParametersWhenGettingMemObjectInfoThenValueSizeRetIsNotUpdated) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0x1234;
    auto retVal = buffer->getMemObjectInfo(
        0,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, sizeReturned);
}

TEST_F(GetMemObjectInfo, GivenMemTypeWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_TYPE,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_mem_object_type), sizeReturned);

    cl_mem_object_type objectType = 0;
    retVal = buffer->getMemObjectInfo(
        CL_MEM_TYPE,
        sizeof(cl_mem_object_type),
        &objectType,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), objectType);
}

TEST_F(GetMemObjectInfo, GivenMemFlagsWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_mem_flags memFlags = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(memFlags), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        sizeof(memFlags),
        &memFlags,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), memFlags);
}

TEST_F(GetMemObjectInfo, GivenMemSizeWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    size_t memSize = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(memSize), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        sizeof(memSize),
        &memSize,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getSize(), memSize);
}

TEST_F(GetMemObjectInfo, GivenMemHostPtrWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    void *hostPtr = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(hostPtr), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        sizeof(hostPtr),
        &hostPtr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getHostPtr(), hostPtr);
}

TEST_F(GetMemObjectInfo, GivenMemContextWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    MockContext context;
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create(&context));

    size_t sizeReturned = 0;
    cl_context contextReturned = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_CONTEXT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(contextReturned), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_CONTEXT,
        sizeof(contextReturned),
        &contextReturned,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&context, contextReturned);
}

TEST_F(GetMemObjectInfo, GivenMemUsesSvmPointerWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create());

    size_t sizeReturned = 0;
    cl_bool usesSVMPointer = false;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_SVM_POINTER,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(usesSVMPointer), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_SVM_POINTER,
        sizeof(usesSVMPointer),
        &usesSVMPointer,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), usesSVMPointer);
}

TEST_F(GetMemObjectInfo, GivenBufferWithMemUseHostPtrAndMemTypeWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto hostPtr = clSVMAlloc(BufferDefaults::context, CL_MEM_READ_WRITE, BufferUseHostPtr<>::sizeInBytes, 64);
        ASSERT_NE(nullptr, hostPtr);
        cl_int retVal;

        auto buffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
            BufferUseHostPtr<>::sizeInBytes,
            hostPtr,
            retVal);

        size_t sizeReturned = 0;
        cl_bool usesSVMPointer = false;

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            0,
            nullptr,
            &sizeReturned);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(usesSVMPointer), sizeReturned);

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            sizeof(usesSVMPointer),
            &usesSVMPointer,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), usesSVMPointer);

        delete buffer;
        clSVMFree(BufferDefaults::context, hostPtr);
    }
}

TEST_F(GetMemObjectInfo, GivenMemOffsetWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    size_t offset = false;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_OFFSET,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(offset), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_OFFSET,
        sizeof(offset),
        &offset,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, offset);
}

TEST_F(GetMemObjectInfo, GivenMemAssociatedMemobjectWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_mem object = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_ASSOCIATED_MEMOBJECT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(object), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_ASSOCIATED_MEMOBJECT,
        sizeof(object),
        &object,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, object);
}

TEST_F(GetMemObjectInfo, GivenMemMapCountWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_uint mapCount = static_cast<uint32_t>(-1);
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_MAP_COUNT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mapCount), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_MAP_COUNT,
        sizeof(mapCount),
        &mapCount,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mapCount), sizeReturned);
}

TEST_F(GetMemObjectInfo, GivenMemReferenceCountWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_uint refCount = static_cast<uint32_t>(-1);
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_REFERENCE_COUNT,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(refCount), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_REFERENCE_COUNT,
        sizeof(refCount),
        &refCount,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(refCount), sizeReturned);
}

TEST_F(GetMemObjectInfo, GivenValidBufferWhenGettingCompressionOfMemObjectThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());
    auto graphicsAllocation = buffer->getMultiGraphicsAllocation().getDefaultGraphicsAllocation();

    size_t sizeReturned = 0;
    cl_bool usesCompression{};
    cl_int retVal{};
    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(sizeof(cl_bool), sizeReturned);

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        sizeReturned,
        &usesCompression,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(cl_bool{CL_TRUE}, usesCompression);

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), false);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        sizeReturned,
        &usesCompression,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(cl_bool{CL_FALSE}, usesCompression);
}

class GetMemObjectInfoLocalMemory : public GetMemObjectInfo {
    using GetMemObjectInfo::SetUp;

  public:
    void SetUp() override {
        dbgRestore = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableLocalMemory.set(1);
        GetMemObjectInfo::SetUp();

        delete BufferDefaults::context;
        BufferDefaults::context = new MockContext(pClDevice, true);
    }

    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
};

TEST_F(GetMemObjectInfoLocalMemory, givenLocalMemoryEnabledWhenNoZeroCopySvmAllocationUsedThenBufferAllocationInheritsZeroCopyFlag) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        auto hostPtr = clSVMAlloc(BufferDefaults::context, CL_MEM_READ_WRITE, BufferUseHostPtr<>::sizeInBytes, 64);
        ASSERT_NE(nullptr, hostPtr);
        cl_int retVal;

        auto buffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
            BufferUseHostPtr<>::sizeInBytes,
            hostPtr,
            retVal);

        size_t sizeReturned = 0;
        cl_bool usesSVMPointer = false;

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            0,
            nullptr,
            &sizeReturned);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(sizeof(usesSVMPointer), sizeReturned);

        retVal = buffer->getMemObjectInfo(
            CL_MEM_USES_SVM_POINTER,
            sizeof(usesSVMPointer),
            &usesSVMPointer,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), usesSVMPointer);

        EXPECT_TRUE(buffer->isMemObjWithHostPtrSVM());
        EXPECT_FALSE(buffer->isMemObjZeroCopy());

        delete buffer;
        clSVMFree(BufferDefaults::context, hostPtr);
    }
}

TEST_F(GetMemObjectInfo, GivenValidInternalHandleWhenGettingMemObjectInfoThenHandleIsReturned) {
    auto graphicsAllocation = new MockGraphicsAllocation();
    MemObj memObj(BufferDefaults::context, CL_MEM_OBJECT_BUFFER, {}, CL_MEM_COPY_HOST_PTR, 0,
                  64, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAllocation), true, false, false);
    size_t sizeReturned = 0;
    uint64_t internalHandle = std::numeric_limits<uint64_t>::max();
    auto retVal = memObj.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(internalHandle), sizeReturned);

    graphicsAllocation->internalHandle = 0xf00d;
    graphicsAllocation->peekInternalHandleResult = 0;

    retVal = memObj.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        sizeof(internalHandle),
        &internalHandle,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(internalHandle, graphicsAllocation->internalHandle);
}

TEST_F(GetMemObjectInfo, GivenFailureOnGettingInternalHandleWhenGettingMemObjectInfoThenErrorIsReturned) {
    auto graphicsAllocation = new MockGraphicsAllocation();
    MemObj memObj(BufferDefaults::context, CL_MEM_OBJECT_BUFFER, {}, CL_MEM_COPY_HOST_PTR, 0,
                  64, nullptr, nullptr, GraphicsAllocationHelper::toMultiGraphicsAllocation(graphicsAllocation), true, false, false);
    size_t sizeReturned = 0;
    uint64_t internalHandle = std::numeric_limits<uint64_t>::max();
    auto retVal = memObj.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(internalHandle), sizeReturned);

    graphicsAllocation->peekInternalHandleResult = -1;

    retVal = memObj.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        sizeof(internalHandle),
        &internalHandle,
        &sizeReturned);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);

    graphicsAllocation->peekInternalHandleResult = 1;

    retVal = memObj.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        sizeof(internalHandle),
        &internalHandle,
        &sizeReturned);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
}

TEST_F(GetMemObjectInfo, GivenTwoRootDevicesInReverseOrderWhenGettingMemObjectInfoThenCorrectHandleIsReturned) {
    auto device1 = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, 1));
    auto device2 = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, 0));

    ClDeviceVector devices;
    devices.push_back(device1.get());
    devices.push_back(device2.get());
    auto context = MockContext(devices);

    auto graphicsAllocation1 = new MockGraphicsAllocation();
    auto graphicsAllocation2 = new MockGraphicsAllocation(1, nullptr, 0);
    graphicsAllocation2->internalHandle = 0xf00d;
    graphicsAllocation2->peekInternalHandleResult = 0;

    MultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(graphicsAllocation1);
    multiGraphicsAllocation.addAllocation(graphicsAllocation2);

    MemObj buffer(&context, CL_MEM_OBJECT_BUFFER, {}, CL_MEM_COPY_HOST_PTR, 0, 64, nullptr, nullptr, std::move(multiGraphicsAllocation), true, false, false);

    size_t sizeReturned = 0;
    uint64_t internalHandle = std::numeric_limits<uint64_t>::max();

    auto retVal = buffer.getMemObjectInfo(
        CL_MEM_ALLOCATION_HANDLE_INTEL,
        sizeof(internalHandle),
        &internalHandle,
        &sizeReturned);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(internalHandle), sizeReturned);
    EXPECT_EQ(internalHandle, graphicsAllocation2->internalHandle);
}
