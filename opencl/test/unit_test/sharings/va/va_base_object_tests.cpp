/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/accelerators/intel_accelerator.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/base_object.h"
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
    IntelAccelerator,
    //Context,
    //Program,
    //Kernel,
    //Sampler
    //others...
    MockCommandQueue>
    BaseObjectTypes;

TYPED_TEST_CASE(VABaseObjectTests, BaseObjectTypes);

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
