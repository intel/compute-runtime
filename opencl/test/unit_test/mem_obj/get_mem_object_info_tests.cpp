/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class GetMemObjectInfo : public ::testing::Test, public PlatformFixture, public ClDeviceFixture {
    using ClDeviceFixture::SetUp;
    using PlatformFixture::SetUp;

  public:
    void SetUp() override {
        PlatformFixture::SetUp();
        ClDeviceFixture::SetUp();
        BufferDefaults::context = new MockContext;
    }

    void TearDown() override {
        delete BufferDefaults::context;
        ClDeviceFixture::TearDown();
        PlatformFixture::TearDown();
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

    cl_mem_object_type object_type = 0;
    retVal = buffer->getMemObjectInfo(
        CL_MEM_TYPE,
        sizeof(cl_mem_object_type),
        &object_type,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), object_type);
}

TEST_F(GetMemObjectInfo, GivenMemFlagsWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    cl_mem_flags mem_flags = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mem_flags), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_FLAGS,
        sizeof(mem_flags),
        &mem_flags,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), mem_flags);
}

TEST_F(GetMemObjectInfo, GivenMemSizeWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    size_t mem_size = 0;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(mem_size), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_SIZE,
        sizeof(mem_size),
        &mem_size,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getSize(), mem_size);
}

TEST_F(GetMemObjectInfo, GivenMemHostPtrWhenGettingMemObjectInfoThenCorrectValueIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    void *host_ptr = nullptr;
    auto retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(host_ptr), sizeReturned);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_HOST_PTR,
        sizeof(host_ptr),
        &host_ptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(buffer->getHostPtr(), host_ptr);
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

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmClientContext(), true);

    retVal = buffer->getMemObjectInfo(
        CL_MEM_USES_COMPRESSION_INTEL,
        sizeReturned,
        &usesCompression,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(cl_bool{CL_TRUE}, usesCompression);

    MockBuffer::setAllocationType(graphicsAllocation, pDevice->getRootDeviceEnvironment().getGmmClientContext(), false);

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
        DebugManager.flags.EnableLocalMemory.set(1);
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
