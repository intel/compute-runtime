/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_dg1.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"

namespace NEO {

class DrmMemoryManagerFixtureDg1 : public DrmMemoryManagerFixture {
  public:
    DrmMockCustomDg1 *mockDg1;

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        mockDg1 = new DrmMockCustomDg1;
        DrmMemoryManagerFixture::SetUp(mockDg1, true);
    }

    void TearDown() override {
        mockDg1->testIoctlsDg1();
        DrmMemoryManagerFixture::TearDown();
    }
};
} // namespace NEO
