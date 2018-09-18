/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/device/device.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "unit_tests/mocks/mock_device_factory.h"
#include "unit_tests/os_interface/linux/drm_mock.h"
#include "unit_tests/gen_common/test.h"
#include "gtest/gtest.h"

namespace OCLRT {
void pushDrmMock(Drm *mock);
void popDrmMock();
}; // namespace OCLRT

using namespace OCLRT;

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
    ExecutionEnvironment executionEnvironment;
};
