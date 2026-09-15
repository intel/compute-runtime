/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/sharings/va/leo_cl_va_api.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/image/image_imp.h"

#include "CL/cl.h"

#include <limits>
#include <utility>

namespace NEO {
namespace LEO {
namespace ult {

struct VaContextFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

using VaSupportedFormatsTest = VaContextFixture;

TEST_F(VaSupportedFormatsTest, givenNullContextWhenGettingSupportedFormatsThenInvalidContextIsReturned) {
    cl_uint numImageFormats = 1;
    auto retVal = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(nullptr, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                               0, 0, nullptr, &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(VaSupportedFormatsTest, givenNonContextObjectWhenGettingSupportedFormatsThenInvalidContextIsReturned) {
    auto notAContext = reinterpret_cast<cl_context>(static_cast<cl_device_id>(clDevice));
    cl_uint numImageFormats = 1;
    auto retVal = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(notAContext, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                               0, 0, nullptr, &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(VaSupportedFormatsTest, givenContextWithoutVaSharingWhenGettingSupportedFormatsThenInvalidContextIsReturned) {
    cl_uint numImageFormats = 1;
    auto retVal = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(static_cast<cl_context>(leoContext.get()), CL_MEM_READ_WRITE,
                                                               CL_MEM_OBJECT_IMAGE2D, 0, 0, nullptr, &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(0u, numImageFormats);
}

using VaEnqueueSharedObjectsTest = VaContextFixture;

TEST_F(VaEnqueueSharedObjectsTest, givenInvalidCommandQueueWhenAcquiringMediaSurfacesThenInvalidCommandQueueIsReturned) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueAcquireVA_APIMediaSurfacesINTEL(nullptr, 0, nullptr, 0, nullptr, nullptr));
}

TEST_F(VaEnqueueSharedObjectsTest, givenInvalidCommandQueueWhenReleasingMediaSurfacesThenInvalidCommandQueueIsReturned) {
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, clEnqueueReleaseVA_APIMediaSurfacesINTEL(nullptr, 0, nullptr, 0, nullptr, nullptr));
}

struct MockL0Image : public L0::ImageImp {
    MockL0Image() {
        this->imgInfo.surfaceFormat = &mockSurfaceFormat;
        this->imgInfo.imgDesc.imageType = NEO::ImageType::image2D;
    }

    ze_result_t initialize(L0::Device *device, const ze_image_desc_t *desc) override {
        return ZE_RESULT_SUCCESS;
    }

    void copySurfaceStateToSSH(void *surfaceStateHeap, uint32_t surfaceStateOffset, uint32_t bindlessSlot,
                               bool isMediaBlockArg, uint32_t mipLevel) override {}

    void setImagePlane(NEO::ImagePlane plane) { this->imgInfo.plane = plane; }

    NEO::SurfaceFormatInfo mockSurfaceFormat{};
};

struct VaImagePlaneTest : public VaContextFixture {
    Image *createImageWithPlane(NEO::ImagePlane plane) {
        auto l0Image = new MockL0Image;
        l0Image->setImagePlane(plane);

        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        cl_image_format format{CL_R, CL_UNORM_INT8};
        return new Image(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, l0Image->toHandle(), nullptr, nullptr,
                         false, format, nullptr);
    }

    cl_uint queryVaPlane(Image *image) {
        cl_uint plane = std::numeric_limits<cl_uint>::max();
        size_t retSize = 0;
        EXPECT_EQ(CL_SUCCESS, image->getImageInfo(CL_IMAGE_VA_API_PLANE_INTEL, sizeof(cl_uint), &plane, &retSize));
        EXPECT_EQ(sizeof(cl_uint), retSize);
        return plane;
    }
};

TEST_F(VaImagePlaneTest, givenImageWithoutPlaneWhenQueryingVaPlaneThenZeroIsReturned) {
    auto image = createImageWithPlane(NEO::ImagePlane::noPlane);

    EXPECT_EQ(0u, queryVaPlane(image));

    delete image;
}

TEST_F(VaImagePlaneTest, givenPlanarImageWhenQueryingVaPlaneThenZeroBasedPlaneIndexIsReturned) {
    const std::pair<NEO::ImagePlane, cl_uint> expectedPlanes[] = {
        {NEO::ImagePlane::planeY, 0u},
        {NEO::ImagePlane::planeU, 1u},
        {NEO::ImagePlane::planeV, 2u}};

    for (const auto &[imagePlane, expectedIndex] : expectedPlanes) {
        auto image = createImageWithPlane(imagePlane);

        EXPECT_EQ(expectedIndex, queryVaPlane(image));

        delete image;
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
