/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/base_object.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/sharing_factory.h"
#include "gtest/gtest.h"

namespace OCLRT {

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
} // namespace OCLRT
