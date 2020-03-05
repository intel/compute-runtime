/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class GetMemObjectInfo : public ::testing::Test, public PlatformFixture, public DeviceFixture {
    using DeviceFixture::SetUp;
    using PlatformFixture::SetUp;

  public:
    void SetUp() override {
        PlatformFixture::SetUp();
        DeviceFixture::SetUp();
        BufferDefaults::context = new MockContext;
    }

    void TearDown() override {
        delete BufferDefaults::context;
        DeviceFixture::TearDown();
        PlatformFixture::TearDown();
    }
};

TEST_F(GetMemObjectInfo, InvalidFlags_returnsError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<>::create());

    size_t sizeReturned = 0;
    auto retVal = buffer->getMemObjectInfo(
        0,
        0,
        nullptr,
        &sizeReturned);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(GetMemObjectInfo, MEM_TYPE) {
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

TEST_F(GetMemObjectInfo, MEM_FLAGS) {
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

TEST_F(GetMemObjectInfo, MEM_SIZE) {
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

TEST_F(GetMemObjectInfo, MEM_HOST_PTR) {
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

TEST_F(GetMemObjectInfo, MEM_CONTEXT) {
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

TEST_F(GetMemObjectInfo, MEM_USES_SVM_POINTER_FALSE) {
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

TEST_F(GetMemObjectInfo, MEM_USES_SVM_POINTER_TRUE) {
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

TEST_F(GetMemObjectInfo, MEM_OFFSET) {
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

TEST_F(GetMemObjectInfo, MEM_ASSOCIATED_MEMOBJECT) {
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

TEST_F(GetMemObjectInfo, MEM_MAP_COUNT) {
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

TEST_F(GetMemObjectInfo, MEM_REFERENCE_COUNT) {
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
