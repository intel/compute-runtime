/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_neo.h"
#include "drm/i915_drm.h"
#include "runtime/helpers/options.h"
#include "test.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include <vector>

namespace OCLRT {

static std::vector<Drm *> drmMockStack;

class DrmMockDefault : public DrmMock {
  public:
    DrmMockDefault() : DrmMock() {
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

struct static_init : public DrmMockDefault {
    static_init() : DrmMockDefault() { drmMockStack.push_back(this); }
};

static static_init s;

void pushDrmMock(Drm *mock) { drmMockStack.push_back(mock); }

void popDrmMock() { drmMockStack.pop_back(); }

Drm::~Drm() { fd = -1; }

Drm *Drm::get(int32_t deviceOrdinal) {
    // We silently skip deviceOrdinal
    EXPECT_EQ(deviceOrdinal, 0);
    return drmMockStack[drmMockStack.size() - 1];
}

Drm *Drm::create(int32_t deviceOrdinal) {
    // We silently skip deviceOrdinal
    EXPECT_EQ(deviceOrdinal, 0);

    return drmMockStack[drmMockStack.size() - 1];
}

void Drm::closeDevice(int32_t deviceOrdinal) { drmMockStack[drmMockStack.size() - 1]->fd = -1; }
} // namespace OCLRT
