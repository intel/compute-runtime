/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmTest, whenRegisterResourceClassesCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto result = drmMock.registerResourceClasses();
    EXPECT_FALSE(result);
}

TEST(DrmTest, whenResourceClassIsUsedToIndexClassNamesThenCorrectNamesAreReturned) {
    EXPECT_STREQ(Drm::classNames[static_cast<uint32_t>(Drm::ResourceClass::Elf)], "I915_CLASS_ELF_FILE");
    EXPECT_STREQ(Drm::classNames[static_cast<uint32_t>(Drm::ResourceClass::Isa)], "I915_CLASS_ISA");
    EXPECT_STREQ(Drm::classNames[static_cast<uint32_t>(Drm::ResourceClass::ContextSaveArea)], "I915_CLASS_CONTEXT_SAVE_AREA");
    EXPECT_STREQ(Drm::classNames[static_cast<uint32_t>(Drm::ResourceClass::ModuleHeapDebugArea)], "I915_CLASS_MODULE_HEAP_DEBUG_AREA");
    EXPECT_STREQ(Drm::classNames[static_cast<uint32_t>(Drm::ResourceClass::SbaTrackingBuffer)], "I915_CLASS_SBA_TRACKING_BUFFER");
}

TEST(DrmTest, whenRegisterResourceCalledThenImplementationIsEmpty) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    auto handle = drmMock.registerResource(Drm::ResourceClass::MaxSize, nullptr, 0);
    EXPECT_EQ(0u, handle);
    drmMock.unregisterResource(handle);
    EXPECT_EQ(0u, drmMock.ioctlCallsCount);
}
