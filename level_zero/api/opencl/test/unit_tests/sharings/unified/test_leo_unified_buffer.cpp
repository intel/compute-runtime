/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_buffer.h"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing.h"
#include "level_zero/api/opencl/source/sharings/unified/leo_unified_sharing_types.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct UnifiedBufferTest : public Test<OclFixture> {
    void SetUp() override {
        // The import target must be its own allocation: a pooled chunk shares the pool's SVM data, which the buffer frees.
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        capturingContext->getDriverHandleCallBase = true;
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
        leoContext->registerSharing(new UnifiedSharingFunctions());
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    void *allocateImportTarget() {
        ze_device_mem_alloc_desc_t deviceAllocDesc{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        void *ptr = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocDevice(context->toHandle(), &deviceAllocDesc, bufferSize, 0, device->toHandle(), &ptr));
        return ptr;
    }

    UnifiedSharingMemoryDescription description(UnifiedSharingHandleType type) const {
        UnifiedSharingMemoryDescription extMem{};
        extMem.type = type;
        extMem.handle = reinterpret_cast<void *>(0x10);
        extMem.size = bufferSize;
        return extMem;
    }

    static constexpr size_t bufferSize = 64u;
    DebugManagerStateRestore restorer;
    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

TEST_F(UnifiedBufferTest, givenLinuxFdHandleWhenCreatingSharedUnifiedBufferThenFileDescriptorIsImportedOnDefaultDeviceAndUnifiedBufferHandlerIsAttached) {
    capturingContext->allocDeviceMemPtr = allocateImportTarget();
    cl_int errcode = CL_INVALID_VALUE;

    auto buffer = UnifiedBuffer::createSharedUnifiedBuffer(leoContext.get(), CL_MEM_READ_WRITE, description(UnifiedSharingHandleType::linuxFd), &errcode);

    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, errcode);
    auto sharingHandler = buffer->peekSharingHandler();
    ASSERT_NE(nullptr, sharingHandler);
    EXPECT_EQ(UnifiedSharingHandleType::linuxFd, static_cast<UnifiedSharing *>(sharingHandler)->getExternalMemoryType());
    ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
    const auto &args = capturingContext->allocDeviceMemArgs[0];
    EXPECT_EQ(clDevice->getL0Handle(), args.hDevice);
    EXPECT_EQ(bufferSize, args.size);
    EXPECT_TRUE(args.fdImport);
    EXPECT_FALSE(args.win32Import);
    EXPECT_EQ(0x10, args.fd);

    buffer->release();
}

TEST_F(UnifiedBufferTest, givenWin32HandleWhenCreatingSharedUnifiedBufferThenWin32HandleIsImportedWithoutNameOnDefaultDeviceAndUnifiedBufferHandlerIsAttached) {
    capturingContext->allocDeviceMemPtr = allocateImportTarget();
    cl_int errcode = CL_INVALID_VALUE;

    auto buffer = UnifiedBuffer::createSharedUnifiedBuffer(leoContext.get(), CL_MEM_READ_WRITE, description(UnifiedSharingHandleType::win32Nt), &errcode);

    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, errcode);
    auto sharingHandler = buffer->peekSharingHandler();
    ASSERT_NE(nullptr, sharingHandler);
    EXPECT_EQ(UnifiedSharingHandleType::win32Nt, static_cast<UnifiedSharing *>(sharingHandler)->getExternalMemoryType());
    ASSERT_EQ(1u, capturingContext->allocDeviceMemArgs.count());
    const auto &args = capturingContext->allocDeviceMemArgs[0];
    EXPECT_EQ(clDevice->getL0Handle(), args.hDevice);
    EXPECT_EQ(bufferSize, args.size);
    EXPECT_TRUE(args.win32Import);
    EXPECT_FALSE(args.fdImport);
    EXPECT_EQ(reinterpret_cast<void *>(0x10), args.win32Handle);
    EXPECT_EQ(nullptr, args.win32Name);

    buffer->release();
}

TEST_F(UnifiedBufferTest, givenImportFailureWhenCreatingSharedUnifiedBufferThenInvalidMemObjectIsReturned) {
    capturingContext->allocDeviceMemResult = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    cl_int errcode = CL_SUCCESS;

    auto buffer = UnifiedBuffer::createSharedUnifiedBuffer(leoContext.get(), CL_MEM_READ_WRITE, description(UnifiedSharingHandleType::linuxFd), &errcode);

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, errcode);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
