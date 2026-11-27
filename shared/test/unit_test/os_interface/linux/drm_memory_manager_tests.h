/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>
#include <sys/mman.h>

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;
using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;
using DrmMemoryManagerWithExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithoutQuietIoctlExpectation>;
using DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest = Test<DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation>;
using DrmMemoryManagerUSMHostAllocationTests = Test<DrmMemoryManagerFixture>;

namespace NEO {

inline AllocationProperties createAllocationProperties(uint32_t rootDeviceIndex, size_t size, bool forcePin) {
    MockAllocationProperties properties(rootDeviceIndex, size);
    properties.alignment = MemoryConstants::preferredAlignment;
    properties.flags.forcePin = forcePin;
    return properties;
}

inline size_t capturedMremapSize = 0u;
inline void *capturedMremapNewAddress = nullptr;
inline uint32_t capturedMremapCalls = 0u;
inline bool failCapturedMremap = false;
inline void *capturingMremapFixed(void *oldAddress, size_t size, void *newAddress) noexcept {
    capturedMremapSize = size;
    capturedMremapNewAddress = newAddress;
    capturedMremapCalls++;
    return failCapturedMremap ? MAP_FAILED : newAddress;
}
inline void resetCapturedMremap() {
    capturedMremapSize = 0u;
    capturedMremapNewAddress = nullptr;
    capturedMremapCalls = 0u;
    failCapturedMremap = false;
}
template <typename DrmMockT>
void enableMmapWindowRelocation(DrmMockT *drm) {
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isMmapWindowRelocationSupportedResult = true;
    drm->ioctlHelper = std::move(ioctlHelper);
}
} // namespace NEO
