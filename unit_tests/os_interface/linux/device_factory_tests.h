/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
