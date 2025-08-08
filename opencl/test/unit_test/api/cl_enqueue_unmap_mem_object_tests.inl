/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include <memory>

namespace CpuIntrinsicsTests {
extern std::atomic<uintptr_t> lastClFlushedPtr;
extern std::atomic<uint32_t> clFlushCounter;
extern std::atomic<uint32_t> sfenceCounter;
} // namespace CpuIntrinsicsTests

namespace NEO {

using ClEnqueueUnmapMemObjTests = ApiTests;

TEST_F(ClEnqueueUnmapMemObjTests, givenValidAddressWhenUnmappingThenReturnSuccess) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueUnmapMemObjTests, GivenQueueIncapableWhenUnmappingBufferThenInvalidOperationIsReturned) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL);
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(ClEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnCpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClEnqueueUnmapMemObjTests, givenZeroCopyWithoutCoherencyAllowedWhenMapAndUnmapThenFlushCachelines) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowZeroCopyWithoutCoherency.set(1);
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferAllocHostPtr<>>::create(pContext));
    EXPECT_TRUE(buffer->mappingOnCpuAllowed());
    buffer->isMemObjZeroCopy();
    cl_int retVal = CL_SUCCESS;

    CpuIntrinsicsTests::clFlushCounter.store(0u);
    CpuIntrinsicsTests::lastClFlushedPtr.store(0u);
    CpuIntrinsicsTests::sfenceCounter.store(0u);

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CpuIntrinsicsTests::clFlushCounter.load(), 1u);
    EXPECT_EQ(CpuIntrinsicsTests::lastClFlushedPtr.load(), reinterpret_cast<uintptr_t>(mappedPtr));
    EXPECT_EQ(CpuIntrinsicsTests::sfenceCounter.load(), 1u);
    CpuIntrinsicsTests::lastClFlushedPtr.store(0u);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(CpuIntrinsicsTests::clFlushCounter.load(), 2u);
    EXPECT_EQ(CpuIntrinsicsTests::lastClFlushedPtr.load(), reinterpret_cast<uintptr_t>(mappedPtr));
    EXPECT_EQ(CpuIntrinsicsTests::sfenceCounter.load(), 2u);

    CpuIntrinsicsTests::clFlushCounter.store(0u);
    CpuIntrinsicsTests::lastClFlushedPtr.store(0u);
    CpuIntrinsicsTests::sfenceCounter.store(0u);
}

TEST_F(ClEnqueueUnmapMemObjTests, givenInvalidAddressWhenUnmappingOnGpuThenReturnError) {
    auto buffer = std::unique_ptr<Buffer>(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    buffer->setSharingHandler(new SharingHandler());
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, buffer.get(), CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        buffer.get(),
        ptrOffset(mappedPtr, buffer->getSize() + 1),
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClEnqueueUnmapMemObjTests, GivenInvalidMemObjectTypeWhenUnmappingImageThenInvalidMemObjectIsReturned) {
    MockContext context{};
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{&context, allocation};
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = clEnqueueMapBuffer(pCommandQueue, &buffer, CL_TRUE, CL_MAP_READ, 0, 1, 0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    buffer.memObjectType = 0x123456;
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        &buffer,
        mappedPtr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

struct ClEnqueueUnmapImageTests : ClEnqueueUnmapMemObjTests,
                                  ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        ClEnqueueUnmapMemObjTests::SetUp();
        const auto imageType = static_cast<cl_mem_object_type>(GetParam());
        this->image.reset(createImage(imageType));
        EXPECT_NE(nullptr, image.get());
    }

    void TearDown() override {
        this->image.reset();
        ClEnqueueUnmapMemObjTests::TearDown();
    }

    Image *createImage(cl_mem_object_type type) {
        switch (type) {
        case CL_MEM_OBJECT_IMAGE1D:
            return Image1dHelperUlt<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
            return Image1dBufferHelperUlt<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            return Image1dArrayHelperUlt<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE2D:
            return Image2dHelperUlt<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
            return Image2dArrayHelperUlt<>::create(pContext);
        case CL_MEM_OBJECT_IMAGE3D:
            return Image3dHelperUlt<>::create(pContext);
        default:
            return nullptr;
        }
    }

    std::unique_ptr<Image> image;

    const size_t origin[3] = {0, 0, 0};
    const size_t region[3] = {1, 1, 1};
    size_t imageRowPitch = 0;
    size_t imageSlicePitch = 0;
};

TEST_P(ClEnqueueUnmapImageTests, GivenValidParametersWhenUnmappingImageThenSuccessIsReturned) {
    void *mappedImage = clEnqueueMapImage(
        pCommandQueue,
        image.get(),
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        image.get(),
        mappedImage,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(ClEnqueueUnmapImageTests, GivenQueueIncapableParametersWhenUnmappingImageThenInvalidOperationIsReturned) {
    void *mappedImage = clEnqueueMapImage(
        pCommandQueue,
        image.get(),
        CL_TRUE,
        CL_MAP_READ,
        origin,
        region,
        &imageRowPitch,
        &imageSlicePitch,
        0,
        nullptr,
        nullptr,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL);
    retVal = clEnqueueUnmapMemObject(
        pCommandQueue,
        image.get(),
        mappedImage,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

INSTANTIATE_TEST_SUITE_P(
    ClEnqueueUnmapImageTests,
    ClEnqueueUnmapImageTests,
    ::testing::Values(
        CL_MEM_OBJECT_IMAGE2D,
        CL_MEM_OBJECT_IMAGE3D,
        CL_MEM_OBJECT_IMAGE2D_ARRAY,
        CL_MEM_OBJECT_IMAGE1D,
        CL_MEM_OBJECT_IMAGE1D_ARRAY,
        CL_MEM_OBJECT_IMAGE1D_BUFFER));

} // namespace NEO