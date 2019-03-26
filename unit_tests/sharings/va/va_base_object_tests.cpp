/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/base_object.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/sharing_factory.h"

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
    Platform,
    IntelAccelerator,
    //Context,
    //Program,
    //Kernel,
    //Sampler
    //others...
    CommandQueue,
    DeviceQueue>
    BaseObjectTypes;

TYPED_TEST_CASE(VABaseObjectTests, BaseObjectTypes);

TYPED_TEST(VABaseObjectTests, commonRuntimeExpectsDispatchTableAtFirstPointerInObject) {
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
