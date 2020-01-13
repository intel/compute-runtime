/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/memory_allocator_multi_device_fixture.h"
#include "opencl/test/unit_test/mocks/linux/mock_drm_memory_manager.h"

using namespace NEO;

void MemoryAllocatorMultiDeviceSystemSpecificFixture::SetUp(ExecutionEnvironment &executionEnvironment) {
    auto memoryManager = static_cast<TestedDrmMemoryManager *>(executionEnvironment.memoryManager.get());
    auto bufferObject = memoryManager->createSharedBufferObject(0u, 10, true, 0u);
    memoryManager->pushSharedBufferObject(bufferObject);
}

void MemoryAllocatorMultiDeviceSystemSpecificFixture::TearDown(ExecutionEnvironment &executionEnvironment) {
    auto memoryManager = static_cast<TestedDrmMemoryManager *>(executionEnvironment.memoryManager.get());
    auto bufferObject = memoryManager->sharingBufferObjects.back();
    memoryManager->eraseSharedBufferObject(bufferObject);
    delete bufferObject;
}
