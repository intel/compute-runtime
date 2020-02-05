/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/execution_environment/execution_environment.h"
#include "core/os_interface/device_factory.h"
#include "runtime/device/device.h"
#include "test.h"
#include "unit_tests/mocks/mock_device_factory.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

namespace NEO {
void pushDrmMock(Drm *mock);
void popDrmMock();
}; // namespace NEO

using namespace NEO;

struct DeviceFactoryLinuxTest : public ::testing::Test {
    void SetUp() override {
        pDrm = new DrmMock;
        pDrm->setGtType(GTTYPE_GT2);
        pushDrmMock(pDrm);
    }

    void TearDown() override {
        popDrmMock();
        delete pDrm;
    }

    DrmMock *pDrm;
    MockExecutionEnvironment executionEnvironment;
};
