/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/unified/unified_buffer.h"
#include "opencl/source/sharings/unified/unified_image.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_fixtures.h"

#include <algorithm>
#include <vector>

using namespace NEO;

template <typename BaseFixture>
struct FdImportFixture : BaseFixture {
    static UnifiedSharingMemoryDescription fdDescription(int fd) {
        UnifiedSharingMemoryDescription desc{};
        desc.handle = reinterpret_cast<void *>(static_cast<uintptr_t>(fd));
        desc.type = UnifiedSharingHandleType::linuxFd;
        desc.size = 4096u;
        return desc;
    }

    static inline std::vector<int> closedFds;

    static int recordClose(int fd) {
        closedFds.push_back(fd);
        return 0;
    }

    static auto closeCount(int fd) {
        return std::ranges::count(closedFds, fd);
    }

    VariableBackup<int (*)(int)> closeBackup{&SysCalls::sysCallsClose, recordClose};
    VariableBackup<std::vector<int>> closedFdsBackup{&closedFds, {}};
};

using UnifiedSharingFdImportTests = FdImportFixture<UnifiedSharingFixture<true, true>>;
using UnifiedSharingFdImportFailingOpenTests = FdImportFixture<UnifiedSharingFixture<true, false>>;

TEST_F(UnifiedSharingFdImportTests, givenFdWhenImportingBufferThenMemoryManagerImportsFdAndFdIsClosedOnce) {
    const int fd = 7;
    cl_int retVal = CL_INVALID_VALUE;
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), 0, fdDescription(fd), &retVal, {}));

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(1u, memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(fd), memoryManager->capturedSharedHandle);
    EXPECT_EQ(1, closeCount(fd));
}

TEST_F(UnifiedSharingFdImportTests, givenFdWhenImportingImageThenMemoryManagerImportsFdAndFdIsClosedOnce) {
    const int fd = 7;
    cl_int retVal = CL_INVALID_VALUE;
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), CL_MEM_READ_WRITE, fdDescription(fd), &Image2dDefaults::imageFormat, &Image2dDefaults::imageDesc, &retVal, {}));

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);
    EXPECT_EQ(1u, memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(fd), memoryManager->capturedSharedHandle);
    EXPECT_EQ(1, closeCount(fd));
}

HWTEST_F(UnifiedSharingFdImportTests, givenImageRejectedAfterImportingFdWhenImportingImageThenAllocationIsFreedAndFdIsNotClosed) {
    struct MockGfxCoreHelper : GfxCoreHelperHw<FamilyType> {
        bool isReducedSurfaceStateInUse(const RootDeviceEnvironment &rootDeviceEnvironment) const override {
            return true;
        }
    };
    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    const int fd = 7;
    cl_image_desc imageDesc = Image2dDefaults::imageDesc;
    imageDesc.num_samples = 16;
    cl_int retVal = CL_SUCCESS;
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), CL_MEM_READ_WRITE, fdDescription(fd), &Image2dDefaults::imageFormat, &imageDesc, &retVal, {}));

    EXPECT_EQ(nullptr, image);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
    EXPECT_EQ(1u, memoryManager->freeGraphicsMemoryCalled);
    EXPECT_EQ(0, closeCount(fd));
}

TEST_F(UnifiedSharingFdImportFailingOpenTests, givenFdThatCannotBeImportedWhenImportingBufferThenFdIsNotClosed) {
    const int fd = 7;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), 0, fdDescription(fd), &retVal, {}));

    EXPECT_EQ(nullptr, buffer);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
    EXPECT_EQ(1u, memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(fd), memoryManager->capturedSharedHandle);
    EXPECT_EQ(0, closeCount(fd));
}

TEST_F(UnifiedSharingFdImportTests, givenNtHandleWhenImportingBufferThenHandleIsImportedAndNotClosed) {
    const int handle = 7;
    auto desc = fdDescription(handle);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal = CL_INVALID_VALUE;
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), 0, desc, &retVal, {}));

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(handle), memoryManager->capturedSharedHandle);
    EXPECT_EQ(0, closeCount(handle));
}

TEST_F(UnifiedSharingFdImportTests, givenSharedHandleWhenImportingBufferThenHandleIsImportedAndNotClosed) {
    const int handle = 7;
    auto desc = fdDescription(handle);
    desc.type = UnifiedSharingHandleType::win32Shared;
    cl_int retVal = CL_INVALID_VALUE;
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), 0, desc, &retVal, {}));

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(handle), memoryManager->capturedSharedHandle);
    EXPECT_EQ(0, closeCount(handle));
}

using UnifiedSharingFdImportMultiRootDeviceTests = FdImportFixture<MultiRootDeviceFixture>;

TEST_F(UnifiedSharingFdImportMultiRootDeviceTests, givenFdWhenImportingBufferForSeveralRootDevicesThenFdIsClosedOnce) {
    const int fd = 7;
    cl_int retVal = CL_INVALID_VALUE;
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), 0, fdDescription(fd), &retVal, {}));

    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, mockMemoryManager->createGraphicsAllocationFromSharedHandleCalled);
    EXPECT_EQ(static_cast<osHandle>(fd), mockMemoryManager->capturedSharedHandle);
    EXPECT_EQ(1, closeCount(fd));
}
