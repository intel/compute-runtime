/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/unified_memory_fixture.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/command_queue/command_queue.h"

namespace NEO {

void UnifiedMemoryAubFixture::setUp() {
    if (PagaFaultManagerTestConfig::disabled) {
        skipped = true;
        GTEST_SKIP();
    }
    AUBFixture::setUp(nullptr);
    if (!platform()->peekExecutionEnvironment()->memoryManager->getPageFaultManager()) {
        skipped = true;
        GTEST_SKIP();
    }
}

void *UnifiedMemoryAubFixture::allocateUSM(InternalMemoryType type) {
    void *ptr = nullptr;
    if (!this->skipped) {
        switch (type) {
        case InternalMemoryType::deviceUnifiedMemory:
            ptr = clDeviceMemAllocINTEL(this->context, this->device, nullptr, dataSize, 0, &retVal);
            break;
        case InternalMemoryType::hostUnifiedMemory:
            ptr = clHostMemAllocINTEL(this->context, nullptr, dataSize, 0, &retVal);
            break;
        case InternalMemoryType::sharedUnifiedMemory:
            ptr = clSharedMemAllocINTEL(this->context, this->device, nullptr, dataSize, 0, &retVal);
            break;
        default:
            ptr = new char[dataSize];
            break;
        }
        EXPECT_EQ(retVal, CL_SUCCESS);
        EXPECT_NE(ptr, nullptr);
    }
    return ptr;
}

void UnifiedMemoryAubFixture::freeUSM(void *ptr, InternalMemoryType type) {
    if (!this->skipped) {
        switch (type) {
        case InternalMemoryType::deviceUnifiedMemory:
        case InternalMemoryType::hostUnifiedMemory:
        case InternalMemoryType::sharedUnifiedMemory:
            retVal = clMemFreeINTEL(this->context, ptr);
            break;
        default:
            delete[] static_cast<char *>(ptr);
            break;
        }
        EXPECT_EQ(retVal, CL_SUCCESS);
    }
}

void UnifiedMemoryAubFixture::writeToUsmMemory(std::vector<char> data, void *ptr, InternalMemoryType type) {
    if (!this->skipped) {
        switch (type) {
        case InternalMemoryType::deviceUnifiedMemory:
            retVal = clEnqueueMemcpyINTEL(this->pCmdQ, true, ptr, data.data(), dataSize, 0, nullptr, nullptr);
            break;
        default:
            std::copy(data.begin(), data.end(), static_cast<char *>(ptr));
            break;
        }
        EXPECT_EQ(retVal, CL_SUCCESS);
    }
}

} // namespace NEO
