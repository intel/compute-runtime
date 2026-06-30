/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/mem_obj_helper.h"
#include "level_zero/api/opencl/source/platform/platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

struct LeoMemObjApiFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
    }

    void TearDown() override {
        context.reset();
        Test<OclFixture>::TearDown();
    }

    void setSupportsImages(bool supported) {
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.supportsImages = supported;
    }

    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
};

using GetSupportedImageFormatsTest = LeoMemObjApiFixture;

TEST_F(GetSupportedImageFormatsTest, givenImagesNotSupportedWhenGetSupportedImageFormatsThenZeroFormatsReturned) {
    setSupportsImages(false);

    cl_uint numImageFormats = 0xdeadu;
    auto retVal = clGetSupportedImageFormats(context.get(), CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE3D,
                                             0, nullptr, &numImageFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(GetSupportedImageFormatsTest, givenImagesSupportedWhenGetSupportedImageFormatsThenNonZeroFormatsReturned) {
    setSupportsImages(true);

    cl_uint numImageFormats = 0;
    auto retVal = clGetSupportedImageFormats(context.get(), CL_MEM_WRITE_ONLY, CL_MEM_OBJECT_IMAGE3D,
                                             0, nullptr, &numImageFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, numImageFormats);
}

using MemObjHelperTest = LeoMemObjApiFixture;

TEST_F(MemObjHelperTest, givenContextDeviceWhenValidateMemoryPropertiesForBufferThenDeviceIsAssociated) {
    auto &neoDeviceRef = context->getClDevice()->getDevice();
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &neoDeviceRef);

    bool valid = MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, CL_MEM_READ_WRITE, 0, *context);
    EXPECT_TRUE(valid);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
