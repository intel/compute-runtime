/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/device/device.h"
#include "core/execution_environment/execution_environment.h"
#include "core/os_interface/device_factory.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

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
