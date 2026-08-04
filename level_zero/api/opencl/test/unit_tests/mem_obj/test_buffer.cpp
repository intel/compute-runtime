/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct BufferDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(void *usmPtr, bool usesExternalHandle = false, void *cpuPtr = nullptr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, cpuPtr, 64u, usesExternalHandle);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

TEST_F(BufferDestructorTest, givenBufferWithoutSvmAndWithoutExternalHandleWhenDestroyedThenZeMemFreeExtIsCalledWithBlockingPolicy) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(false);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(usmPtr, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithCpuAllocationAndSvmWhenDestroyedThenMemObjCallsZeMemFreeExtWithBlockingPolicy) {
    uint64_t dummyCpuStorage = 0u;
    auto buffer = createBuffer(&dummyStorage, false, &dummyCpuStorage);
    buffer->setUsesSvm(true);

    delete buffer;

    ASSERT_EQ(1u, capturingContext->freeMemExtArgs.count());
    const auto &freeMem = capturingContext->freeMemExtArgs[0];
    EXPECT_EQ(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE, freeMem.freePolicy);
    EXPECT_EQ(&dummyCpuStorage, freeMem.ptr);
}

TEST_F(BufferDestructorTest, givenBufferWithSvmWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    auto buffer = createBuffer(usmPtr);
    buffer->setUsesSvm(true);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

TEST_F(BufferDestructorTest, givenBufferWithExternalHandleWhenDestroyedThenZeMemFreeExtIsNotCalled) {
    void *usmPtr = &dummyStorage;
    constexpr bool externalHandle = true;
    auto buffer = createBuffer(usmPtr, externalHandle);

    delete buffer;

    EXPECT_FALSE(capturingContext->freeMemExtArgs.wasCalled());
}

struct BufferZeroCopyForHostPtrTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        memoryManager = driverHandle->getMemoryManager();
        hostPtr = alignedMalloc(bufferSize, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, hostPtr);
    }

    void TearDown() override {
        alignedFree(hostPtr);
        Test<OclFixture>::TearDown();
    }

    static constexpr size_t bufferSize = 2 * MemoryConstants::pageSize;

    MemoryManager *memoryManager = nullptr;
    void *hostPtr = nullptr;
};

TEST_F(BufferZeroCopyForHostPtrTest, givenCacheLineAlignedHostPtrAndSizeThenZeroCopyIsAllowed) {
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, memoryManager));
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, MemoryConstants::cacheLineSize, memoryManager));

    auto cacheLineAlignedPtr = ptrOffset(hostPtr, MemoryConstants::cacheLineSize);
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(cacheLineAlignedPtr, MemoryConstants::cacheLineSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenNullHostPtrThenZeroCopyIsNotAllowed) {
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(nullptr, bufferSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenHostPtrNotAlignedToCacheLineThenZeroCopyIsNotAllowed) {
    auto misalignedPtr = ptrOffset(hostPtr, 1u);
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(misalignedPtr, MemoryConstants::cacheLineSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenSizeNotAlignedToCacheLineThenZeroCopyIsNotAllowed) {
    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, MemoryConstants::cacheLineSize - 1u, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenZeroCopyForUseHostPtrDisabledThenZeroCopyIsNotAllowed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableZeroCopyForUseHostPtr.set(true);

    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, memoryManager));
}

TEST_F(BufferZeroCopyForHostPtrTest, givenHostPtrBelowMinAddressRestrictionThenZeroCopyIsNotAllowed) {
    MockAllocSysMemAgnosticMemoryManager restrictedMemoryManager{*neoDevice->getExecutionEnvironment()};
    restrictedMemoryManager.testRestrictions.minAddress = castToUint64(hostPtr) + MemoryConstants::pageSize;
    restrictedMemoryManager.ptrRestrictions = &restrictedMemoryManager.testRestrictions;

    EXPECT_FALSE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, &restrictedMemoryManager));

    restrictedMemoryManager.testRestrictions.minAddress = castToUint64(hostPtr);
    EXPECT_TRUE(Buffer::isZeroCopyAllowedForHostPtr(hostPtr, bufferSize, &restrictedMemoryManager));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
