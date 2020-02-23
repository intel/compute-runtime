/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "test.h"

#include "gtest/gtest.h"

namespace NEO {
extern Drm **pDrmToReturnFromCreateFunc;
}; // namespace NEO

using namespace NEO;

struct DeviceFactoryLinuxTest : public ::testing::Test {
    void SetUp() override {
        pDrm = new DrmMock;
        pDrmToReturnFromCreateFunc = reinterpret_cast<Drm **>(&pDrm);
        pDrm->setGtType(GTTYPE_GT2);
    }

    void TearDown() override {
    }
    VariableBackup<Drm **> drmBackup{&pDrmToReturnFromCreateFunc};
    DrmMock *pDrm;
    MockExecutionEnvironment executionEnvironment;
};
