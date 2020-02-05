/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/linux/drm_neo.h"
#include "test.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include "drm/i915_drm.h"

#include <vector>

namespace NEO {

static std::vector<Drm *> drmMockStack;

class DrmMockDefault : public DrmMock {
  public:
    DrmMockDefault(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        StoredRetVal = 0;
        StoredRetValForDeviceID = 0;
        StoredRetValForEUVal = 0;
        StoredRetValForSSVal = 0;
        StoredRetValForDeviceRevID = 0;
        StoredRetValForPooledEU = 0;
        StoredRetValForMinEUinPool = 0;
        setGtType(GTTYPE_GT1);
    }
};

struct static_init {
    static_init() : rootDeviceEnvironment(executionEnvironment), drmMockDefault(rootDeviceEnvironment) { drmMockStack.push_back(&drmMockDefault); }
    ExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment;
    DrmMockDefault drmMockDefault;
};

static static_init s;

void pushDrmMock(Drm *mock) { drmMockStack.push_back(mock); }

void popDrmMock() { drmMockStack.pop_back(); }

Drm::~Drm() = default;

Drm *Drm::get(int32_t deviceOrdinal) {
    // We silently skip deviceOrdinal
    EXPECT_EQ(deviceOrdinal, 0);

    return drmMockStack[drmMockStack.size() - 1];
}

Drm *Drm::create(int32_t deviceOrdinal, RootDeviceEnvironment &rootDeviceEnvironment) {
    // We silently skip deviceOrdinal
    EXPECT_EQ(deviceOrdinal, 0);

    return drmMockStack[drmMockStack.size() - 1];
}

void Drm::closeDevice(int32_t deviceOrdinal) {}
} // namespace NEO
