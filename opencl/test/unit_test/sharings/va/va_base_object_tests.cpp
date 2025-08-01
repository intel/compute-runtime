/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

namespace NEO {

template <typename TypeParam>
struct VABaseObjectTests : public ::testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

typedef ::testing::Types<
    MockPlatform,
    MockCommandQueue>
    BaseObjectTypes;

TYPED_TEST_SUITE(VABaseObjectTests, BaseObjectTypes);

TYPED_TEST(VABaseObjectTests, GivenCommonRuntimeThenDispatchTableAtFirstPointerInObject) {
    TypeParam objectDrv;

    // Automatic downcasting to _cl_type *.
    typename TypeParam::BaseType *objectCL = &objectDrv;
    sharingFactory.fillGlobalDispatchTable();

    // Common runtime casts to generic type assuming
    // the dispatch table is the first ptr in the structure
    auto genericObject = reinterpret_cast<ClDispatch *>(objectCL);
    EXPECT_EQ(reinterpret_cast<void *>(clCreateFromVA_APIMediaSurfaceINTEL),
              genericObject->dispatch.crtDispatch->clCreateFromVA_APIMediaSurfaceINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueAcquireVA_APIMediaSurfacesINTEL),
              genericObject->dispatch.crtDispatch->clEnqueueAcquireVA_APIMediaSurfacesINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clEnqueueReleaseVA_APIMediaSurfacesINTEL),
              genericObject->dispatch.crtDispatch->clEnqueueReleaseVA_APIMediaSurfacesINTEL);

    EXPECT_EQ(reinterpret_cast<void *>(clGetDeviceIDsFromVA_APIMediaAdapterINTEL),
              genericObject->dispatch.crtDispatch->clGetDeviceIDsFromVA_APIMediaAdapterINTEL);
}
} // namespace NEO
