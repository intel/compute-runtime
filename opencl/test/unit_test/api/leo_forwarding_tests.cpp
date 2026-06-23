/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/leo_forwarding.h"

namespace NEO {

static void resetL0Library() {
    leoTeardown();
    leoSetup();
}

TEST(AdditionalExtension, whenCheckIsLEOEnabledWithFlagSetTo1ThenReturnTrue) {
    DebugManagerStateRestore restorer;
    resetL0Library();

    debugManager.flags.EnableLEO.set(1);
    EXPECT_TRUE(isLEOEnabled());

    leoTeardown();
}

TEST(AdditionalExtension, whenCheckIsLEOEnabledWithFlagSetTo0ThenReturnFalse) {
    DebugManagerStateRestore restorer;
    resetL0Library();

    debugManager.flags.EnableLEO.set(0);
    EXPECT_FALSE(isLEOEnabled());

    leoTeardown();
}

TEST(AdditionalExtension, whenCheckIsLEOEnabledWithDefaultFlagAndNoLibraryThenReturnFalse) {
    DebugManagerStateRestore restorer;
    resetL0Library();

    EXPECT_FALSE(isLEOEnabled());

    leoTeardown();
}

static cl_int CL_API_CALL mockClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms) {
    if (numPlatforms) {
        *numPlatforms = 42u;
    }
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClGetPlatformIDsNoPlatforms(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms) {
    if (numPlatforms) {
        *numPlatforms = 0u;
    }
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClGetPlatformInfo(cl_platform_id platform, cl_platform_info paramName,
                                                size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) {
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClGetDeviceIDs(cl_platform_id platform, cl_device_type deviceType,
                                             cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
    if (numDevices) {
        *numDevices = 7u;
    }
    return CL_SUCCESS;
}

static void *CL_API_CALL mockClGetExtensionFunctionAddress(const char *funcName) {
    return reinterpret_cast<void *>(0x123);
}

static cl_int CL_API_CALL mockClEnqueueMarkerWithSyncObjectINTEL(cl_command_queue commandQueue, cl_event *event, cl_context *context) {
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClGetCLObjectInfoINTEL(cl_mem memObj, void *pResourceInfo) {
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClGetCLEventInfoINTEL(cl_event event, void **pSyncInfoHandleRet, cl_context *pClContextRet) {
    return CL_SUCCESS;
}

static cl_int CL_API_CALL mockClReleaseGlSharedEventINTEL(cl_event event) {
    return CL_SUCCESS;
}

struct L0LibraryLoadTest : public ::testing::Test {
    void SetUp() override {
        resetL0Library();
    }
    void TearDown() override {
        leoTeardown();
    }
    DebugManagerStateRestore restorer;
};

TEST_F(L0LibraryLoadTest, givenEnableLEOWhenLoadingLibraryFailsThenForwardFunctionsReturnDefaults) {
    debugManager.flags.EnableLEO.set(1);

    auto savedLoadFunc = OsLibrary::loadFunc;
    OsLibrary::loadFunc = [](const OsLibraryCreateProperties &properties) -> OsLibrary * {
        return nullptr;
    };

    EXPECT_TRUE(isLEOEnabled());

    cl_uint numPlatforms = 1u;
    auto retVal = forwardClGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numPlatforms);

    retVal = forwardClGetPlatformInfo(nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);

    retVal = forwardClGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, nullptr);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);

    auto funcAddr = forwardClGetExtensionFunctionAddress("someName");
    EXPECT_EQ(nullptr, funcAddr);

    retVal = forwardClEnqueueMarkerWithSyncObjectINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClGetCLObjectInfoINTEL(nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClGetCLEventInfoINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClReleaseGlSharedEventINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenEnableLEOWhenLoadingLibrarySucceedsThenForwardFunctionsResolveAndForwardCalls) {
    debugManager.flags.EnableLEO.set(1);

    auto mockLibrary = new MockOsLibraryCustom(nullptr, true);
    mockLibrary->procMap["clGetPlatformIDs"] = reinterpret_cast<void *>(mockClGetPlatformIDs);
    mockLibrary->procMap["clGetPlatformInfo"] = reinterpret_cast<void *>(mockClGetPlatformInfo);
    mockLibrary->procMap["clGetDeviceIDs"] = reinterpret_cast<void *>(mockClGetDeviceIDs);
    mockLibrary->procMap["clGetExtensionFunctionAddress"] = reinterpret_cast<void *>(mockClGetExtensionFunctionAddress);
    mockLibrary->procMap["clEnqueueMarkerWithSyncObjectINTEL"] = reinterpret_cast<void *>(mockClEnqueueMarkerWithSyncObjectINTEL);
    mockLibrary->procMap["clGetCLObjectInfoINTEL"] = reinterpret_cast<void *>(mockClGetCLObjectInfoINTEL);
    mockLibrary->procMap["clGetCLEventInfoINTEL"] = reinterpret_cast<void *>(mockClGetCLEventInfoINTEL);
    mockLibrary->procMap["clReleaseGlSharedEventINTEL"] = reinterpret_cast<void *>(mockClReleaseGlSharedEventINTEL);

    auto savedLoadFunc = OsLibrary::loadFunc;
    MockOsLibrary::loadLibraryNewObject = mockLibrary;
    OsLibrary::loadFunc = MockOsLibrary::load;

    EXPECT_TRUE(isLEOEnabled());

    cl_uint numPlatforms = 0u;
    auto retVal = forwardClGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(42u, numPlatforms);

    retVal = forwardClGetPlatformInfo(nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint numDevices = 0u;
    retVal = forwardClGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(7u, numDevices);

    auto funcAddr = forwardClGetExtensionFunctionAddress("someName");
    EXPECT_EQ(reinterpret_cast<void *>(0x123), funcAddr);

    retVal = forwardClEnqueueMarkerWithSyncObjectINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = forwardClGetCLObjectInfoINTEL(nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = forwardClGetCLEventInfoINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = forwardClReleaseGlSharedEventINTEL(nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenEnableLEOWhenLibraryLoadedButNotAllSymbolsResolvedThenUnresolvedForwardsReturnDefaults) {
    debugManager.flags.EnableLEO.set(1);

    auto mockLibrary = new MockOsLibraryCustom(nullptr, true);
    mockLibrary->procMap["clGetPlatformIDs"] = reinterpret_cast<void *>(mockClGetPlatformIDs);

    auto savedLoadFunc = OsLibrary::loadFunc;
    MockOsLibrary::loadLibraryNewObject = mockLibrary;
    OsLibrary::loadFunc = MockOsLibrary::load;

    EXPECT_TRUE(isLEOEnabled());

    cl_uint numPlatforms = 0u;
    auto retVal = forwardClGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(42u, numPlatforms);

    retVal = forwardClGetPlatformInfo(nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);

    retVal = forwardClGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 0, nullptr, nullptr);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);

    auto funcAddr = forwardClGetExtensionFunctionAddress("someName");
    EXPECT_EQ(nullptr, funcAddr);

    retVal = forwardClEnqueueMarkerWithSyncObjectINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClGetCLObjectInfoINTEL(nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClGetCLEventInfoINTEL(nullptr, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    retVal = forwardClReleaseGlSharedEventINTEL(nullptr);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenDefaultFlagWhenLibraryReturnsPlatformsThenIsLEOEnabledReturnsTrue) {
    auto mockLibrary = new MockOsLibraryCustom(nullptr, true);
    mockLibrary->procMap["clGetPlatformIDs"] = reinterpret_cast<void *>(mockClGetPlatformIDs);

    auto savedLoadFunc = OsLibrary::loadFunc;
    MockOsLibrary::loadLibraryNewObject = mockLibrary;
    OsLibrary::loadFunc = MockOsLibrary::load;

    EXPECT_TRUE(isLEOEnabled());
    EXPECT_TRUE(l0ForwardingState->hasPlatforms);
    EXPECT_NE(nullptr, l0ForwardingState->library.get());

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenDefaultFlagWhenLibraryReturnsNoPlatformsThenIsLEOEnabledReturnsFalseAndLibraryIsUnloaded) {
    auto mockLibrary = new MockOsLibraryCustom(nullptr, true);
    mockLibrary->procMap["clGetPlatformIDs"] = reinterpret_cast<void *>(mockClGetPlatformIDsNoPlatforms);

    auto savedLoadFunc = OsLibrary::loadFunc;
    MockOsLibrary::loadLibraryNewObject = mockLibrary;
    OsLibrary::loadFunc = MockOsLibrary::load;

    EXPECT_FALSE(isLEOEnabled());
    EXPECT_FALSE(l0ForwardingState->hasPlatforms);
    EXPECT_EQ(nullptr, l0ForwardingState->library.get());

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenDefaultFlagWhenLibraryLoadFailsThenIsLEOEnabledReturnsFalse) {
    auto savedLoadFunc = OsLibrary::loadFunc;
    OsLibrary::loadFunc = [](const OsLibraryCreateProperties &properties) -> OsLibrary * {
        return nullptr;
    };

    EXPECT_FALSE(isLEOEnabled());

    OsLibrary::loadFunc = savedLoadFunc;
}

TEST_F(L0LibraryLoadTest, givenFlag1WhenLibraryReturnsNoPlatformsThenIsLEOEnabledStillReturnsTrueAndLibraryIsNotUnloaded) {
    debugManager.flags.EnableLEO.set(1);

    auto mockLibrary = new MockOsLibraryCustom(nullptr, true);
    mockLibrary->procMap["clGetPlatformIDs"] = reinterpret_cast<void *>(mockClGetPlatformIDsNoPlatforms);

    auto savedLoadFunc = OsLibrary::loadFunc;
    MockOsLibrary::loadLibraryNewObject = mockLibrary;
    OsLibrary::loadFunc = MockOsLibrary::load;

    EXPECT_TRUE(isLEOEnabled());
    EXPECT_NE(nullptr, l0ForwardingState->library.get());

    OsLibrary::loadFunc = savedLoadFunc;
}

} // namespace NEO
