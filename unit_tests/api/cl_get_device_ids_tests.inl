/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/variable_backup.h"

using namespace OCLRT;

typedef api_tests clGetDeviceIDsTests;

namespace ULT {

TEST_F(clGetDeviceIDsTests, NumDevices) {
    cl_uint numDevices = 0;

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, DeviceIDs) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, nullPlatform) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, invalidDeviceType) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, 0x0f00, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE_TYPE, retVal);
}

TEST_F(clGetDeviceIDsTests, invalidVal) {
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceIDsTests, invalidPlatform) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];
    uint32_t trash[6] = {0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    cl_platform_id p = reinterpret_cast<cl_platform_id>(trash);

    retVal = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetDeviceIDsTests, devTypeAll) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_ALL, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, devTypeDefault) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_DEFAULT, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, cpuDevices) {
    cl_uint numDevices = 0;

    retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_CPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(numDevices, (cl_uint)0);
}

} // namespace ULT
namespace OCLRT {
extern bool overrideDeviceWithDefaultHardwareInfo;
extern bool overrideCommandStreamReceiverCreation;

TEST(MultiDeviceTests, givenCreateMultipleDevicesAndLimitAmountOfReturnedDevicesFlagWhenClGetDeviceIdsIsCalledThenLowerValueIsReturned) {
    platformImpl.reset(nullptr);
    overrideCommandStreamReceiverCreation = true;
    VariableBackup<bool> overrideHelper(&overrideDeviceWithDefaultHardwareInfo, false);
    DeviceFactoryCleaner cleaner;
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CreateMultipleDevices.set(2);
    DebugManager.flags.LimitAmountOfReturnedDevices.set(1);
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numDevices);
}
} // namespace OCLRT
