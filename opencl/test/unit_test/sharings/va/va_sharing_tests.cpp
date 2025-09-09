/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/va/va_device.h"
#include "opencl/source/sharings/va/va_sharing.h"
#include "opencl/source/sharings/va/va_surface.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/sharings/va/mock_va_sharing.h"

#include "gtest/gtest.h"

#include <va/va_backend.h>

namespace NEO {
class Context;
} // namespace NEO

using namespace NEO;

class VaSharingTests : public ::testing::Test, public PlatformFixture {
  public:
    void SetUp() override {
        rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
        PlatformFixture::setUp();
        vaSharing = new MockVaSharing;
        context.setSharingFunctions(&vaSharing->sharingFunctions);
        vaSharing->sharingFunctions.querySupportedVaImageFormats(VADisplay(1));
        vaSharing->updateAcquiredHandle(sharingHandle);
        sharedImg = nullptr;
        sharedClMem = nullptr;
    }

    void TearDown() override {
        if (sharedImg) {
            delete sharedImg;
        }
        context.releaseSharingFunctions(SharingType::VA_SHARING);
        delete vaSharing;
        PlatformFixture::tearDown();
    }

    void updateAcquiredHandle(unsigned int handle) {
        sharingHandle = handle;
        vaSharing->updateAcquiredHandle(sharingHandle);
    }

    void createMediaSurface(cl_uint plane = 0, cl_mem_flags flags = CL_MEM_READ_WRITE) {
        sharedClMem = clCreateFromVA_APIMediaSurfaceINTEL(&context, flags, &vaSurfaceId, plane, &errCode);
        ASSERT_NE(nullptr, sharedClMem);
        EXPECT_EQ(CL_SUCCESS, errCode);
        sharedImg = castToObject<Image>(sharedClMem);
        ASSERT_NE(sharedImg, nullptr);
    }

    uint32_t rootDeviceIndex = 0;
    Image *sharedImg = nullptr;
    cl_mem sharedClMem = nullptr;
    MockContext context{};
    MockVaSharing *vaSharing = nullptr;
    VASurfaceID vaSurfaceId = 0u;
    VAImage vaImage = {};
    cl_int errCode = -1;
    unsigned int sharingHandle = 1u;
};

TEST(VaSharingTest, givenVASharingFunctionsObjectWhenFunctionsAreCalledThenCallsAreRedirectedToVaFunctionPointers) {
    unsigned int handle = 0u;
    VASurfaceID vaSurfaceId = 0u;
    VAImage vaImage = {};
    VADRMPRIMESurfaceDescriptor vaDrmPrimeSurfaceDesc = {};

    class VASharingFunctionsGlobalFunctionPointersMock : public VASharingFunctions {
      public:
        VASharingFunctionsGlobalFunctionPointersMock() : VASharingFunctions(nullptr) {
            initMembers();
        }

        bool vaDisplayIsValidCalled = false;
        bool vaDeriveImageCalled = false;
        bool vaDestroyImageCalled = false;
        bool vaSyncSurfaceCalled = false;
        bool vaGetLibFuncCalled = false;
        bool vaExtGetSurfaceHandleCalled = false;
        bool vaExportSurfaceHandleCalled = false;
        bool vaQueryImageFormatsCalled = false;
        bool vaMaxNumImageFormatsCalled = false;

        void initMembers() {
            vaDisplayIsValidPFN = mockVaDisplayIsValid;
            vaDeriveImagePFN = mockVaDeriveImage;
            vaDestroyImagePFN = mockVaDestroyImage;
            vaSyncSurfacePFN = mockVaSyncSurface;
            vaGetLibFuncPFN = mockVaGetLibFunc;
            vaExtGetSurfaceHandlePFN = mockExtGetSurfaceHandle;
            vaExportSurfaceHandlePFN = mockExportSurfaceHandle;
            vaQueryImageFormatsPFN = mockVaQueryImageFormats;
            vaMaxNumImageFormatsPFN = mockVaMaxNumImageFormats;
        }

        static VASharingFunctionsGlobalFunctionPointersMock *getInstance(bool release) {
            static VASharingFunctionsGlobalFunctionPointersMock *vaSharingFunctions = nullptr;
            if (!vaSharingFunctions) {
                vaSharingFunctions = new VASharingFunctionsGlobalFunctionPointersMock;
            } else if (release) {
                delete vaSharingFunctions;
                vaSharingFunctions = nullptr;
            }
            return vaSharingFunctions;
        }

        static int mockVaDisplayIsValid(VADisplay vaDisplay) {
            getInstance(false)->vaDisplayIsValidCalled = true;
            return 1;
        };

        static VAStatus mockVaDeriveImage(VADisplay vaDisplay, VASurfaceID vaSurface, VAImage *vaImage) {
            getInstance(false)->vaDeriveImageCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static VAStatus mockVaDestroyImage(VADisplay vaDisplay, VAImageID vaImageId) {
            getInstance(false)->vaDestroyImageCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static VAStatus mockVaSyncSurface(VADisplay vaDisplay, VASurfaceID vaSurface) {
            getInstance(false)->vaSyncSurfaceCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static void *mockVaGetLibFunc(VADisplay vaDisplay, const char *func) {
            getInstance(false)->vaGetLibFuncCalled = true;
            return nullptr;
        };

        static VAStatus mockExtGetSurfaceHandle(VADisplay vaDisplay, VASurfaceID *vaSurface, unsigned int *handleId) {
            getInstance(false)->vaExtGetSurfaceHandleCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static VAStatus mockExportSurfaceHandle(VADisplay vaDisplay, VASurfaceID vaSurface, uint32_t memType, uint32_t flags, void *descriptor) {
            getInstance(false)->vaExportSurfaceHandleCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static VAStatus mockVaQueryImageFormats(VADisplay vaDisplay, VAImageFormat *formatList, int *numFormats) {
            getInstance(false)->vaQueryImageFormatsCalled = true;
            return VA_STATUS_SUCCESS;
        };

        static int mockVaMaxNumImageFormats(VADisplay vaDisplay) {
            getInstance(false)->vaMaxNumImageFormatsCalled = true;
            return 0;
        };
    };
    auto vaSharingFunctions = VASharingFunctionsGlobalFunctionPointersMock::getInstance(false);
    EXPECT_TRUE(vaSharingFunctions->isValidVaDisplay());
    EXPECT_EQ(0, vaSharingFunctions->deriveImage(vaSurfaceId, &vaImage));
    EXPECT_EQ(0, vaSharingFunctions->destroyImage(vaImage.image_id));
    EXPECT_EQ(0, vaSharingFunctions->syncSurface(vaSurfaceId));
    EXPECT_TRUE(nullptr == vaSharingFunctions->getLibFunc("funcName"));
    EXPECT_EQ(0, vaSharingFunctions->extGetSurfaceHandle(&vaSurfaceId, &handle));
    EXPECT_EQ(0, vaSharingFunctions->exportSurfaceHandle(vaSurfaceId, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, 0, &vaDrmPrimeSurfaceDesc));
    int numFormats = 0;
    EXPECT_EQ(0, vaSharingFunctions->queryImageFormats(VADisplay(1), nullptr, &numFormats));
    EXPECT_EQ(0, vaSharingFunctions->maxNumImageFormats(VADisplay(1)));

    EXPECT_EQ(0u, handle);

    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaDisplayIsValidCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaDeriveImageCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaDestroyImageCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaSyncSurfaceCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaGetLibFuncCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaExtGetSurfaceHandleCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaExportSurfaceHandleCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaQueryImageFormatsCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaMaxNumImageFormatsCalled);

    VASharingFunctionsGlobalFunctionPointersMock::getInstance(true);
}

TEST_F(VaSharingTests, givenMockVaWithExportSurfaceHandlerWhenVaSurfaceIsCreatedThenCallHandlerWithDrmPrime2ToGetSurfaceFormatsInDescriptor) {
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    for (int plane = 0; plane < 2; plane++) {
        auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(
            &context, &vaSharing->sharingFunctions, CL_MEM_READ_WRITE, 0, &vaSurfaceId, plane, &errCode));
        ASSERT_NE(nullptr, vaSurface);

        auto handler = vaSurface->peekSharingHandler();
        ASSERT_NE(nullptr, handler);

        auto vaHandler = static_cast<VASharing *>(handler);
        EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->sharingFunctions);

        auto &sharingFunctions = vaSharing->sharingFunctions;
        EXPECT_FALSE(sharingFunctions.deriveImageCalled);
        EXPECT_FALSE(sharingFunctions.destroyImageCalled);

        EXPECT_TRUE(sharingFunctions.exportSurfaceHandleCalled);

        EXPECT_EQ(static_cast<uint32_t>(VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2), sharingFunctions.receivedSurfaceMemType);
        EXPECT_EQ(static_cast<uint32_t>(VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS), sharingFunctions.receivedSurfaceFlags);

        if (plane == 0) {
            EXPECT_EQ(256u, vaSurface->getImageDesc().image_width);
            EXPECT_EQ(256u, vaSurface->getImageDesc().image_height);
        }

        if (plane == 1) {
            EXPECT_EQ(128u, vaSurface->getImageDesc().image_width);
            EXPECT_EQ(128u, vaSurface->getImageDesc().image_height);

            SurfaceOffsets surfaceOffsets;
            vaSurface->getSurfaceOffsets(surfaceOffsets);
            auto vaSurfaceDesc = sharingFunctions.mockVaSurfaceDesc;
            EXPECT_EQ(vaSurfaceDesc.layers[1].offset[0], surfaceOffsets.offset);
            EXPECT_EQ(0u, surfaceOffsets.xOffset);
            EXPECT_EQ(0u, surfaceOffsets.yOffset);
            EXPECT_EQ(vaSurfaceDesc.layers[1].offset[0] / vaSurfaceDesc.layers[1].pitch[0], surfaceOffsets.yOffsetForUVplane);
        }

        EXPECT_TRUE(vaSurface->isTiledAllocation());
        EXPECT_EQ(8u, vaSurface->getGraphicsAllocation(rootDeviceIndex)->peekSharedHandle());
    }
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedThenMemObjectHasVaHandler) {
    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                      CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(4096u, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, graphicsAllocation->peekSharedHandle());

    EXPECT_EQ(4096u, vaSurface->getSize());

    auto handler = vaSurface->peekSharingHandler();
    ASSERT_NE(nullptr, handler);

    auto vaHandler = static_cast<VASharing *>(handler);
    EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->sharingFunctions);

    EXPECT_EQ(1u, vaSharing->sharingFunctions.acquiredVaHandle);

    EXPECT_TRUE(vaSharing->sharingFunctions.deriveImageCalled);
    EXPECT_TRUE(vaSharing->sharingFunctions.destroyImageCalled);
    EXPECT_TRUE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    size_t paramSize = 0;
    void *paramValue = nullptr;
    handler->getMemObjectInfo(paramSize, paramValue);
    EXPECT_EQ(sizeof(VASurfaceID *), paramSize);
    VASurfaceID **paramSurfaceId = reinterpret_cast<VASurfaceID **>(paramValue);
    EXPECT_EQ(vaSurfaceId, **paramSurfaceId);

    delete vaSurface;
}

TEST_F(VaSharingTests, givenSupportedFourccFormatWhenIsSupportedPlanarFormatThenSuccessIsReturned) {
    EXPECT_TRUE(VASurface::isSupportedPlanarFormat(VA_FOURCC_P010));
    EXPECT_TRUE(VASurface::isSupportedPlanarFormat(VA_FOURCC_P016));
    EXPECT_TRUE(VASurface::isSupportedPlanarFormat(VA_FOURCC_NV12));
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    EXPECT_TRUE(VASurface::isSupportedPlanarFormat(VA_FOURCC_RGBP));
}

TEST_F(VaSharingTests, givenSupportedPackedFormatWhenIsSupportedPlanarFormatThenFailIsReturned) {
    EXPECT_FALSE(VASurface::isSupportedPlanarFormat(VA_FOURCC_YUY2));
    EXPECT_FALSE(VASurface::isSupportedPlanarFormat(VA_FOURCC_Y210));
    EXPECT_FALSE(VASurface::isSupportedPlanarFormat(VA_FOURCC_ARGB));
}

TEST_F(VaSharingTests, givenSupportedFourccFormatWhenIsSupportedPackedFormatThenSuccessIsReturned) {
    EXPECT_TRUE(VASurface::isSupportedPackedFormat(VA_FOURCC_YUY2));
    EXPECT_TRUE(VASurface::isSupportedPackedFormat(VA_FOURCC_Y210));
    EXPECT_TRUE(VASurface::isSupportedPackedFormat(VA_FOURCC_ARGB));
}

TEST_F(VaSharingTests, givenSupportedPlanarFormatWhenIsSupportedPackedFormatThenFailIsReturned) {
    EXPECT_FALSE(VASurface::isSupportedPackedFormat(VA_FOURCC_NV12));
}

TEST_F(VaSharingTests, givenValidYUY2SurfaceWhenGetSurfaceDescriptionThenCLSuccessIsReturnedAndDataAreSet) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_YUY2;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_YUYV, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_YUY2;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.height, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.width, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0].fd), surfaceInfo.sharedHandle);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc, surfaceInfo.imageFourcc);
    EXPECT_FALSE(surfaceInfo.imgInfo.linearStorage);
}

TEST_F(VaSharingTests, givenValidY210SurfaceWhenGetSurfaceDescriptionThenCLSuccessIsReturnedAndDataAreSet) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_Y210;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_Y210, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;
    vaSharing->sharingFunctions.derivedImageFormatBpp = 32;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_Y210;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.height, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.width, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0].fd), surfaceInfo.sharedHandle);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc, surfaceInfo.imageFourcc);
    EXPECT_FALSE(surfaceInfo.imgInfo.linearStorage);
}

TEST_F(VaSharingTests, givenValidYUY2SurfaceWithInvalidPlaneNumberWhenGetSurfaceDescriptionThenFailIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_YUY2;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_YUYV, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_YUY2;

    SharedSurfaceInfo surfaceInfo{};
    surfaceInfo.plane = 1;
    EXPECT_EQ(VA_STATUS_ERROR_INVALID_PARAMETER, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
}

TEST_F(VaSharingTests, givenValidY210SurfaceWithInvalidPlaneNumberWhenGetSurfaceDescriptionThenFailIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_Y210;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_Y210, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;
    vaSharing->sharingFunctions.derivedImageFormatBpp = 32;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_Y210;

    SharedSurfaceInfo surfaceInfo{};
    surfaceInfo.plane = 1;
    EXPECT_EQ(VA_STATUS_ERROR_INVALID_PARAMETER, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
}

TEST_F(VaSharingTests, givenValidPlanarSurfaceWithPlaneSetWhenGetSurfaceDescriptionThenCLSuccessIsReturnedAndDataAreSet) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_NV12;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_YUYV, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {128, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {512, 0, 0, 0}, {1024, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;

    SharedSurfaceInfo surfaceInfo{};
    surfaceInfo.plane = 1;
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.height, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.width, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0].fd), surfaceInfo.sharedHandle);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc, surfaceInfo.imageFourcc);
    EXPECT_FALSE(surfaceInfo.imgInfo.linearStorage);
    EXPECT_EQ(surfaceInfo.imageOffset, vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1].offset[0]);
    EXPECT_EQ(surfaceInfo.imagePitch, vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1].pitch[0]);

    surfaceInfo.plane = 2;
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.height, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.width, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0].fd), surfaceInfo.sharedHandle);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc, surfaceInfo.imageFourcc);
    EXPECT_FALSE(surfaceInfo.imgInfo.linearStorage);
    EXPECT_EQ(surfaceInfo.imageOffset, vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2].offset[0]);
    EXPECT_EQ(surfaceInfo.imagePitch, vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2].pitch[0]);
}

TEST_F(VaSharingTests, givenValidYUY2SurfaceWhenGetSurfaceDescriptionWithDeriveImageThenCLSuccessIsReturnedAndDataAreSet) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_YUY2;
    vaSharing->sharingFunctions.derivedImageHeight = 24;
    vaSharing->sharingFunctions.derivedImageWidth = 24;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageHeight, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageWidth, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.derivedImageFormatFourCC), surfaceInfo.imageFourcc);
}

TEST_F(VaSharingTests, givenValidY210SurfaceWhenGetSurfaceDescriptionWithDeriveImageThenCLSuccessIsReturnedAndDataAreSet) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 32;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_Y210;
    vaSharing->sharingFunctions.derivedImageHeight = 24;
    vaSharing->sharingFunctions.derivedImageWidth = 24;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageHeight, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageWidth, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.derivedImageFormatFourCC), surfaceInfo.imageFourcc);
}

TEST_F(VaSharingTests, givenValidARGBSurfaceWhenGetSurfaceDescriptionWithDeriveImageThenCLSuccessIsReturnedAndDataAreSet) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 32;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_ARGB;
    vaSharing->sharingFunctions.derivedImageHeight = 24;
    vaSharing->sharingFunctions.derivedImageWidth = 24;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageHeight, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageWidth, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.derivedImageFormatFourCC), surfaceInfo.imageFourcc);
}

TEST(VASurface, givenSupportedY210PackedFormatWhenCheckingIfSupportedThenSurfaceDescIsReturned) {
    EXPECT_NE(nullptr, VASurface::getExtendedSurfaceFormatInfo(VA_FOURCC_Y210));
}

TEST_F(VaSharingTests, givenValidSurfaceWithInvalidPlaneNumberWhenGetSurfaceDescriptionWithDeriveImageThenFailIsReturned) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_YUY2;
    vaSharing->sharingFunctions.derivedImageHeight = 24;
    vaSharing->sharingFunctions.derivedImageWidth = 24;

    SharedSurfaceInfo surfaceInfo{};
    surfaceInfo.plane = 1;
    EXPECT_EQ(VA_STATUS_ERROR_INVALID_PARAMETER, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
}

TEST_F(VaSharingTests, givenInvalidSurfaceWhenGetSurfaceDescriptionWithDeriveImageThenFailIsReturned) {
    vaSharing->sharingFunctions.deriveImageReturnStatus = VA_STATUS_ERROR_OPERATION_FAILED;

    SharedSurfaceInfo surfaceInfo{};
    EXPECT_EQ(VA_STATUS_ERROR_OPERATION_FAILED, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
}

TEST_F(VaSharingTests, givenValidPlanarSurfaceWithPlaneSetWhenGetSurfaceDescriptionWithDeriveImageThenCLSuccessIsReturnedAndDataAreSet) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_NV12;
    vaSharing->sharingFunctions.derivedImageHeight = 24;
    vaSharing->sharingFunctions.derivedImageWidth = 24;

    size_t pitch = alignUp(vaSharing->sharingFunctions.derivedImageWidth, 128);
    size_t offset = alignUp(vaSharing->sharingFunctions.derivedImageHeight, 32) * pitch;

    SharedSurfaceInfo surfaceInfo{};
    surfaceInfo.plane = 1;
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageHeight, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageWidth, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.derivedImageFormatFourCC), surfaceInfo.imageFourcc);
    EXPECT_EQ(surfaceInfo.imageOffset, offset);
    EXPECT_EQ(surfaceInfo.imagePitch, pitch);

    surfaceInfo.plane = 2;
    EXPECT_EQ(VA_STATUS_SUCCESS, VASurface::getSurfaceDescription(surfaceInfo, &vaSharing->sharingFunctions, &vaSurfaceId));
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageHeight, surfaceInfo.imgInfo.imgDesc.imageWidth);
    EXPECT_EQ(vaSharing->sharingFunctions.derivedImageWidth, surfaceInfo.imgInfo.imgDesc.imageHeight);
    EXPECT_EQ(static_cast<uint32_t>(vaSharing->sharingFunctions.derivedImageFormatFourCC), surfaceInfo.imageFourcc);
    EXPECT_EQ(surfaceInfo.imageOffset, offset + 1);
    EXPECT_EQ(surfaceInfo.imagePitch, pitch);
}

TEST_F(VaSharingTests, givenValidPlanarSurfaceWithPlaneSetWhenApplyPlanarOptionsThenProperDataAreSet) {
    SharedSurfaceInfo surfaceInfo{};

    // NV12 part

    surfaceInfo.plane = 0;
    surfaceInfo.gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    surfaceInfo.channelOrder = CL_RG;
    surfaceInfo.channelType = CL_UNORM_INT8;

    VASurface::applyPlanarOptions(surfaceInfo, 0, 0, true);

    EXPECT_EQ(surfaceInfo.imgInfo.plane, GMM_PLANE_Y);
    EXPECT_EQ(surfaceInfo.channelOrder, static_cast<cl_channel_order>(CL_R));
    EXPECT_EQ(surfaceInfo.imgInfo.surfaceFormat->gmmSurfaceFormat, GMM_FORMAT_NV12);

    VASurface::applyPlanarOptions(surfaceInfo, 1, 0, true);

    EXPECT_EQ(surfaceInfo.imgInfo.plane, GMM_PLANE_U);
    EXPECT_EQ(surfaceInfo.channelOrder, static_cast<cl_channel_order>(CL_RG));

    // RGBP part

    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    surfaceInfo.imageFourcc = VA_FOURCC_RGBP;

    VASurface::applyPlanarOptions(surfaceInfo, 1, 0, true);

    EXPECT_EQ(surfaceInfo.imgInfo.plane, GMM_PLANE_U);
    EXPECT_EQ(surfaceInfo.channelOrder, static_cast<cl_channel_order>(CL_R));
    EXPECT_EQ(surfaceInfo.channelType, static_cast<cl_channel_type>(CL_UNORM_INT8));

    surfaceInfo.imageFourcc = VA_FOURCC_RGBP;

    VASurface::applyPlanarOptions(surfaceInfo, 2, 0, true);

    EXPECT_EQ(surfaceInfo.imgInfo.plane, GMM_PLANE_V);
    EXPECT_EQ(surfaceInfo.channelOrder, static_cast<cl_channel_order>(CL_R));
    EXPECT_EQ(surfaceInfo.channelType, static_cast<cl_channel_type>(CL_UNORM_INT8));

    // P010 part

    surfaceInfo.imageFourcc = VA_FOURCC_P010;

    VASurface::applyPlanarOptions(surfaceInfo, 1, 0, true);

    EXPECT_EQ(surfaceInfo.channelType, static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(surfaceInfo.imgInfo.surfaceFormat->gmmSurfaceFormat, GMM_FORMAT_P010);

    // P016 part

    surfaceInfo.imageFourcc = VA_FOURCC_P016;

    VASurface::applyPlanarOptions(surfaceInfo, 1, 0, true);

    EXPECT_EQ(surfaceInfo.channelType, static_cast<cl_channel_type>(CL_UNORM_INT16));
    EXPECT_EQ(surfaceInfo.imgInfo.surfaceFormat->gmmSurfaceFormat, GMM_FORMAT_P016);
}

TEST_F(VaSharingTests, givenValidPlanarSurfaceWithInvalidPlaneSetWhenApplyPlanarOptionsThenUnrecoverableIsCalled) {
    SharedSurfaceInfo surfaceInfo{};

    surfaceInfo.imageFourcc = VA_FOURCC_P016;

    EXPECT_THROW(VASurface::applyPlanarOptions(surfaceInfo, 2, 0, true), std::exception);

    EXPECT_THROW(VASurface::applyPlanarOptions(surfaceInfo, 3, 0, true), std::exception);
}

TEST_F(VaSharingTests, givenValidSurfaceWithPlaneSetWhenApplyPlaneSettingsThenProperDataAreSet) {
    SharedSurfaceInfo surfaceInfo{};

    surfaceInfo.imgInfo.imgDesc.imageWidth = 128;
    surfaceInfo.imgInfo.imgDesc.imageHeight = 128;
    surfaceInfo.imageOffset = 24;
    surfaceInfo.imagePitch = 24;

    VASurface::applyPlaneSettings(surfaceInfo, 1u);

    EXPECT_EQ(surfaceInfo.imgInfo.imgDesc.imageHeight, 64u);
    EXPECT_EQ(surfaceInfo.imgInfo.imgDesc.imageWidth, 64u);
    EXPECT_EQ(surfaceInfo.imgInfo.offset, surfaceInfo.imageOffset);
    EXPECT_EQ(surfaceInfo.imgInfo.yOffsetForUVPlane, static_cast<uint32_t>(surfaceInfo.imageOffset / surfaceInfo.imagePitch));

    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    surfaceInfo.imageFourcc = VA_FOURCC_RGBP;

    VASurface::applyPlaneSettings(surfaceInfo, 1u);

    EXPECT_EQ(surfaceInfo.imgInfo.imgDesc.imageHeight, 64u);
    EXPECT_EQ(surfaceInfo.imgInfo.imgDesc.imageWidth, 64u);

    surfaceInfo.imageOffset = 128;

    VASurface::applyPlaneSettings(surfaceInfo, 2u);

    EXPECT_EQ(surfaceInfo.imgInfo.offset, surfaceInfo.imageOffset);
}

TEST_F(VaSharingTests, givenValidPackedFormatWhenApplyPackedOptionsThenSurfaceFormatIsSet) {
    SharedSurfaceInfo surfaceInfoYUY2{};
    surfaceInfoYUY2.imageFourcc = VA_FOURCC_YUY2;

    VASurface::applyPackedOptions(surfaceInfoYUY2);

    EXPECT_EQ(surfaceInfoYUY2.imgInfo.surfaceFormat->gmmSurfaceFormat, GMM_FORMAT_YCRCB_NORMAL);

    SharedSurfaceInfo surfaceInfoY210{};
    surfaceInfoY210.imageFourcc = VA_FOURCC_Y210;

    VASurface::applyPackedOptions(surfaceInfoY210);

    EXPECT_EQ(surfaceInfoY210.imgInfo.surfaceFormat->gmmSurfaceFormat, GMM_FORMAT_Y210);
}

TEST_F(VaSharingTests, givenInvalidPlaneWhenVaSurfaceIsCreatedAndNotRGBPThenUnrecoverableIsCalled) {
    EXPECT_THROW(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                  CL_MEM_READ_WRITE, 0, &vaSurfaceId, 2, &errCode),
                 std::exception);
}

TEST_F(VaSharingTests, givenInvalidPlaneWhenVaSurfaceIsCreatedThenUnrecoverableIsCalled) {
    EXPECT_THROW(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                  CL_MEM_READ_WRITE, 0, &vaSurfaceId, 3, &errCode),
                 std::exception);
}

TEST_F(VaSharingTests, givenInvalidPlaneInputWhenVaSurfaceIsCreatedThenInvalidValueErrorIsReturned) {
    sharedClMem = clCreateFromVA_APIMediaSurfaceINTEL(&context, CL_MEM_READ_WRITE, &vaSurfaceId, 2, &errCode);
    EXPECT_EQ(nullptr, sharedClMem);
    EXPECT_EQ(CL_INVALID_VALUE, errCode);
}

TEST_F(VaSharingTests, givenValidPlaneInputWhenVaSurfaceIsCreatedAndDebugFlagEnabledThenCLSuccessIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_RGBP;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[1] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[2] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 3;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};

    vaSharing->sharingFunctions.derivedImageFormatBpp = 8;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_RGBP;

    sharedClMem = clCreateFromVA_APIMediaSurfaceINTEL(&context, CL_MEM_READ_WRITE, &vaSurfaceId, 2, &errCode);
    EXPECT_NE(nullptr, sharedClMem);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = clReleaseMemObject(sharedClMem);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedWithNotAlignedWidthAndHeightThenSurfaceOffsetsUseAlignedValues) {
    vaSharing->sharingFunctions.derivedImageWidth = 256 + 16;
    vaSharing->sharingFunctions.derivedImageHeight = 512 + 16;
    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                      CL_MEM_READ_WRITE, 0, &vaSurfaceId, 1, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(4096u, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, graphicsAllocation->peekSharedHandle());

    EXPECT_EQ(4096u, vaSurface->getSize());

    auto handler = vaSurface->peekSharingHandler();
    ASSERT_NE(nullptr, handler);

    auto vaHandler = static_cast<VASharing *>(handler);
    EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->sharingFunctions);

    EXPECT_EQ(1u, vaSharing->sharingFunctions.acquiredVaHandle);

    EXPECT_TRUE(vaSharing->sharingFunctions.deriveImageCalled);
    EXPECT_TRUE(vaSharing->sharingFunctions.destroyImageCalled);
    EXPECT_TRUE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    SurfaceOffsets surfaceOffsets;
    uint16_t alignedWidth = alignUp(vaSharing->sharingFunctions.derivedImageWidth, 128);
    uint16_t alignedHeight = alignUp(vaSharing->sharingFunctions.derivedImageHeight, 32);
    uint64_t alignedOffset = alignedWidth * alignedHeight;

    vaSurface->getSurfaceOffsets(surfaceOffsets);
    EXPECT_EQ(alignedHeight, surfaceOffsets.yOffsetForUVplane);
    EXPECT_EQ(alignedOffset, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);

    delete vaSurface;
}

TEST_F(VaSharingTests, givenContextWhenClCreateFromVaApiMediaSurfaceIsCalledThenSurfaceIsReturned) {
    sharedClMem = clCreateFromVA_APIMediaSurfaceINTEL(&context, CL_MEM_READ_WRITE, &vaSurfaceId, 0, &errCode);
    ASSERT_EQ(CL_SUCCESS, errCode);
    ASSERT_NE(nullptr, sharedClMem);

    errCode = clReleaseMemObject(sharedClMem);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenVASurfaceWhenItIsAcquiredTwiceThenAcquireIsNotCalled) {
    createMediaSurface();

    sharedImg->peekSharingHandler()->acquire(sharedImg, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    vaSharing->sharingFunctions.extGetSurfaceHandleCalled = false;
    sharedImg->peekSharingHandler()->acquire(sharedImg, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_FALSE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);
}

TEST_F(VaSharingTests, givenHwCommandQueueWhenEnqueueAcquireIsCalledMultipleTimesThenSharingFunctionAcquireIsNotCalledMultipleTimes) {
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &errCode);
    ASSERT_EQ(CL_SUCCESS, errCode);

    createMediaSurface();

    EXPECT_TRUE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    vaSharing->sharingFunctions.extGetSurfaceHandleCalled = false;
    errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_FALSE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    errCode = clEnqueueReleaseVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_FALSE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    errCode = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenHwCommandQueueWhenAcquireAndReleaseCallsAreMadeWithEventsThenProperCmdTypeIsReturned) {
    cl_event retEvent = nullptr;
    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;

    createMediaSurface();

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &errCode);
    errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, &retEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);
    ASSERT_NE(retEvent, nullptr);

    errCode = clGetEventInfo(retEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_ACQUIRE_VA_API_MEDIA_SURFACES_INTEL), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);

    errCode = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = clEnqueueReleaseVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, &retEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = clGetEventInfo(retEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_RELEASE_VA_API_MEDIA_SURFACES_INTEL), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);

    errCode = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, errCode);
    errCode = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenVaMediaSurfaceWhenGetMemObjectInfoIsCalledThenSurfaceIdIsReturned) {
    vaSurfaceId = 1u;
    createMediaSurface();

    VASurfaceID *retVaSurfaceId = nullptr;
    size_t retSize = 0;
    vaSurfaceId = 0;
    errCode = clGetMemObjectInfo(sharedClMem, CL_MEM_VA_API_MEDIA_SURFACE_INTEL,
                                 sizeof(VASurfaceID *), &retVaSurfaceId, &retSize);

    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(sizeof(VASurfaceID *), retSize);
    EXPECT_EQ(1u, *retVaSurfaceId);
}

TEST_F(VaSharingTests, givenVaMediaSurfaceWhenGetImageInfoIsCalledThenPlaneIsReturned) {
    cl_uint plane = 1u;
    createMediaSurface(plane);

    cl_uint retPlane = 0u;
    size_t retSize = 0;
    errCode = clGetImageInfo(sharedClMem, CL_IMAGE_VA_API_PLANE_INTEL, sizeof(cl_uint), &retPlane, &retSize);

    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(plane, retPlane);
}

TEST_F(VaSharingTests, givenPlaneWhenCreateSurfaceIsCalledThenSetPlaneFields) {
    cl_uint planes[2] = {0, 1};
    updateAcquiredHandle(2);

    for (int i = 0; i < 2; i++) {
        createMediaSurface(planes[i]);

        EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().oclImageFormat.image_channel_data_type == CL_UNORM_INT8);

        EXPECT_EQ(planes[i], sharedImg->getMediaPlaneType());
        if (planes[i] == 0u) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().oclImageFormat.image_channel_order == CL_R);
        } else if (planes[i] == 1) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().oclImageFormat.image_channel_order == CL_RG);
        }

        delete sharedImg;
        sharedImg = nullptr;
    }
}

TEST_F(VaSharingTests, givenSimpleParamsWhenCreateSurfaceIsCalledThenSetImgObject) {
    updateAcquiredHandle(2);

    createMediaSurface(0u);

    EXPECT_TRUE(sharedImg->getImageDesc().buffer == nullptr);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_array_size);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_depth);
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaImage.height, static_cast<unsigned short>(sharedImg->getImageDesc().image_height));
    EXPECT_EQ(vaSharing->sharingFunctions.mockVaImage.width, static_cast<unsigned short>(sharedImg->getImageDesc().image_width));
    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == sharedImg->getImageDesc().image_type);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_slice_pitch);
    EXPECT_NE(0u, sharedImg->getImageDesc().image_row_pitch);
    EXPECT_EQ(0u, sharedImg->getHostPtrSlicePitch());
    EXPECT_NE(0u, sharedImg->getHostPtrRowPitch());
    EXPECT_TRUE(sharedImg->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(sharedImg->getCubeFaceIndex() == __GMM_NO_CUBE_MAP);

    EXPECT_EQ(vaSharing->sharingHandle, sharedImg->getGraphicsAllocation(rootDeviceIndex)->peekSharedHandle());
}

TEST_F(VaSharingTests, givenNonInteropUserSyncContextWhenAcquireIsCalledThenSyncSurface) {
    context.setInteropUserSyncEnabled(false);
    vaSurfaceId = 1u;
    createMediaSurface();
    vaSurfaceId = 0u;

    auto memObj = castToObject<MemObj>(sharedClMem);

    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
    auto ret = memObj->peekSharingHandler()->acquire(sharedImg, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(vaSharing->sharingFunctions.syncSurfaceCalled);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(1u, vaSharing->sharingFunctions.syncedSurfaceID);
}

TEST_F(VaSharingTests, givenInteropUserSyncContextWhenAcquireIsCalledThenDontSyncSurface) {
    context.setInteropUserSyncEnabled(true);

    createMediaSurface();

    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
    sharedImg->peekSharingHandler()->acquire(sharedImg, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
}

TEST_F(VaSharingTests, whenSyncSurfaceFailedThenReturnOutOfResource) {
    vaSharing->sharingFunctions.syncSurfaceReturnStatus = VA_STATUS_ERROR_INVALID_SURFACE;
    createMediaSurface();

    auto ret = sharedImg->peekSharingHandler()->acquire(sharedImg, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(CL_OUT_OF_RESOURCES, ret);
}

TEST_F(VaSharingTests, givenYuvPlaneWhenCreateIsCalledThenChangeWidthAndHeight) {
    cl_uint planeTypes[] = {
        0, // Y
        1  // U
    };

    context.setInteropUserSyncEnabled(true);

    for (int i = 0; i < 2; i++) {
        createMediaSurface(planeTypes[i]);
        size_t retParam;

        errCode = clGetImageInfo(sharedClMem, CL_IMAGE_WIDTH, sizeof(size_t), &retParam, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);
        if (planeTypes[i] == 1) {
            EXPECT_EQ(128u, retParam);
        } else {
            EXPECT_EQ(256u, retParam);
        }

        errCode = clGetImageInfo(sharedClMem, CL_IMAGE_HEIGHT, sizeof(size_t), &retParam, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);
        if (planeTypes[i] == 1) {
            EXPECT_EQ(128u, retParam);
        } else {
            EXPECT_EQ(256u, retParam);
        }
        delete sharedImg;
        sharedImg = nullptr;
    }
}

TEST_F(VaSharingTests, givenContextWhenSharingTableEmptyThenReturnsNullptr) {
    MockContext context;
    context.clearSharingFunctions();
    VASharingFunctions *sharingF = context.getSharing<VASharingFunctions>();
    EXPECT_EQ(sharingF, nullptr);
}

TEST_F(VaSharingTests, givenInValidPlatformWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnFirstDevice) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(nullptr, 0u, nullptr, 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_INVALID_PLATFORM, errCode);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(0u, devices);
}

TEST_F(VaSharingTests, givenP010FormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P010;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P010, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenP010FormatWhenCreatingSharedVaSurfaceForPlane1ThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P010;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 1, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16G16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P010, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenP016FormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P016;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P016, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenP016FormatWhenCreatingSharedVaSurfaceForPlane1ThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P016;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 1, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16G16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P016, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenRGBAFormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.derivedImageFormatBpp = 32;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_ARGB;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RGBA), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8G8B8A8_UNORM_TYPE, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8G8B8A8_UNORM_TYPE, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenInvalidSurfaceWhenCreatingSharedVaSurfaceThenNullptrReturnedAndErrIsSet) {
    vaSharing->sharingFunctions.deriveImageReturnStatus = VA_STATUS_ERROR_OPERATION_FAILED;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 1, &errCode));
    EXPECT_EQ(vaSurface, nullptr);
    EXPECT_EQ(errCode, VA_STATUS_ERROR_OPERATION_FAILED);
}

TEST_F(VaSharingTests, givenYUY2FormatWhenCreatingSharedVaSurfaceThenCorrectFormatIsUsedByImageAndGMM) {
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_YUY2;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[0] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 1;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[0] = {DRM_FORMAT_YUYV, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.width = 24;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.height = 24;
    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_YUY2;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_YUYV_INTEL), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_YCRCB_NORMAL, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_YCRCB_NORMAL, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenNotSupportedFormatWhenCreatingSharedVaSurfaceThenErrorIsReturned) {
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_NV21;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));

    EXPECT_EQ(vaSurface, nullptr);
    EXPECT_EQ(errCode, VA_STATUS_ERROR_INVALID_PARAMETER);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndRGBPFormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_RGBP;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[1] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[2] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 3;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};

    vaSharing->sharingFunctions.derivedImageFormatBpp = 8;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_RGBP;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndRGBPFormatWhenCreatingSharedVaSurfaceForPlane1ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_RGBP;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[1] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[2] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 3;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};

    vaSharing->sharingFunctions.derivedImageFormatBpp = 8;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_RGBP;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 1, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndRGBPFormatWhenCreatingSharedVaSurfaceForPlane2ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_RGBP;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[1] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[2] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 3;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};

    vaSharing->sharingFunctions.derivedImageFormatBpp = 8;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_RGBP;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 2, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenMockVaWithExportSurfaceHandlerAndRGBPWhenVaSurfaceIsCreatedThenCallHandlerWithDrmPrime2ToGetSurfaceFormatsInDescriptor) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    vaSharing->sharingFunctions.haveExportSurfaceHandle = true;

    vaSharing->sharingFunctions.mockVaSurfaceDesc.fourcc = VA_FOURCC_RGBP;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[1] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.objects[2] = {8, 98304, I915_FORMAT_MOD_Y_TILED};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.num_layers = 3;
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[1] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};
    vaSharing->sharingFunctions.mockVaSurfaceDesc.layers[2] = {DRM_FORMAT_R8, 1, {}, {0, 0, 0, 0}, {256, 0, 0, 0}};

    vaSharing->sharingFunctions.derivedImageFormatBpp = 8;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_RGBP;

    for (int plane = 0; plane < 3; plane++) {
        auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(
            &context, &vaSharing->sharingFunctions, CL_MEM_READ_WRITE, 0, &vaSurfaceId, plane, &errCode));
        ASSERT_NE(nullptr, vaSurface);
        auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);

        auto handler = vaSurface->peekSharingHandler();
        ASSERT_NE(nullptr, handler);

        auto vaHandler = static_cast<VASharing *>(handler);
        EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->sharingFunctions);

        auto &sharingFunctions = vaSharing->sharingFunctions;
        EXPECT_FALSE(sharingFunctions.deriveImageCalled);
        EXPECT_FALSE(sharingFunctions.destroyImageCalled);

        EXPECT_TRUE(sharingFunctions.exportSurfaceHandleCalled);

        EXPECT_EQ(static_cast<uint32_t>(VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2), sharingFunctions.receivedSurfaceMemType);
        EXPECT_EQ(static_cast<uint32_t>(VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS), sharingFunctions.receivedSurfaceFlags);

        EXPECT_EQ(256u, vaSurface->getImageDesc().image_width);
        EXPECT_EQ(256u, vaSurface->getImageDesc().image_height);
        if (plane != 0) {
            SurfaceOffsets surfaceOffsets;
            vaSurface->getSurfaceOffsets(surfaceOffsets);
            auto vaSurfaceDesc = sharingFunctions.mockVaSurfaceDesc;
            EXPECT_EQ(vaSurfaceDesc.layers[plane].offset[0], surfaceOffsets.offset);
            EXPECT_EQ(0u, surfaceOffsets.xOffset);
            EXPECT_EQ(0u, surfaceOffsets.yOffset);
            EXPECT_EQ(vaSurfaceDesc.layers[plane].offset[0] / vaSurfaceDesc.layers[plane].pitch[0], surfaceOffsets.yOffsetForUVplane);
        }

        EXPECT_TRUE(vaSurface->isTiledAllocation());
        EXPECT_EQ(8u, graphicsAllocation->peekSharedHandle());
    }
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndNV12FormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.derivedImageFormatBpp = 12;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_NV12;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, 0, &vaSurfaceId, 0, &errCode));
    auto graphicsAllocation = vaSurface->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12, graphicsAllocation->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

using ApiVaSharingTests = VaSharingTests;

TEST_F(ApiVaSharingTests, givenSupportedImageTypeWhenGettingSupportedVAApiFormatsThenCorrectListIsReturned) {
    cl_mem_flags flags[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE};
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    std::vector<std::unique_ptr<VAImageFormat>> supportedFormats;
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_NV12, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_P010, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_P016, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_YUY2, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_Y210, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));
    supportedFormats.push_back(std::make_unique<VAImageFormat>(VAImageFormat{VA_FOURCC_ARGB, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0}));

    for (auto flag : flags) {

        for (auto plane : {0, 1}) {

            cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
                &context,
                flag,
                imageType,
                plane,
                arrayCount(vaApiFormats),
                vaApiFormats,
                &numImageFormats);

            EXPECT_EQ(CL_SUCCESS, result);
            EXPECT_EQ(6u, numImageFormats);
            int i = 0;
            for (auto &format : supportedFormats) {
                EXPECT_EQ(format->fourcc, vaApiFormats[i++].fourcc);
            }
        }
    }
}

TEST_F(ApiVaSharingTests, givenZeroNumEntriesWhenGettingSupportedVAApiFormatsThenNumFormatsIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;

    for (auto plane : {0, 1}) {

        cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
            &context,
            flags,
            imageType,
            plane,
            0,
            nullptr,
            &numImageFormats);

        EXPECT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(6u, numImageFormats);
    }
}

TEST_F(ApiVaSharingTests, givenNullNumImageFormatsWhenGettingSupportedVAApiFormatsThenNumFormatsIsNotDereferenced) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        imageType,
        0,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(ApiVaSharingTests, givenOtherThanImage2DImageTypeWhenGettingSupportedVAApiFormatsThenSuccessAndZeroFormatsAreReturned) {
    cl_mem_flags flags = CL_MEM_KERNEL_READ_AND_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE3D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        imageType,
        0,
        arrayCount(vaApiFormats),
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(ApiVaSharingTests, givenInvalidFlagsWhenGettingSupportedVAApiFormatsThenIvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_NO_ACCESS_INTEL;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        imageType,
        0,
        arrayCount(vaApiFormats),
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(ApiVaSharingTests, givenInvalidContextWhenGettingSupportedVAApiFormatsThenIvalidContextErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    MockContext contextWihtoutVASharing;
    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &contextWihtoutVASharing,
        flags,
        imageType,
        0,
        arrayCount(vaApiFormats),
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_CONTEXT, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST(VaSurface, givenValidPlaneAndFlagsWhenValidatingInputsThenTrueIsReturned) {
    for (cl_uint plane = 0; plane <= 1; plane++) {
        EXPECT_TRUE(VASurface::validate(CL_MEM_READ_ONLY, plane));
        EXPECT_TRUE(VASurface::validate(CL_MEM_WRITE_ONLY, plane));
        EXPECT_TRUE(VASurface::validate(CL_MEM_READ_WRITE, plane));
        EXPECT_TRUE(VASurface::validate(CL_MEM_READ_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, plane));
        EXPECT_TRUE(VASurface::validate(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, plane));
    }
}

TEST(VaSurface, givenInvalidPlaneOrFlagsWhenValidatingInputsThenTrueIsReturned) {
    cl_uint plane = 2;
    EXPECT_FALSE(VASurface::validate(CL_MEM_READ_ONLY, plane));
    EXPECT_FALSE(VASurface::validate(CL_MEM_USE_HOST_PTR, 0));
}

TEST(VaSurface, givenNotSupportedVaFormatsWhenCheckingIfSupportedThenFalseIsReturned) {
    EXPECT_FALSE(VASurface::isSupportedFourCCTwoPlaneFormat(VA_FOURCC_NV11));
    DebugManagerStateRestore restore;
    debugManager.flags.EnableExtendedVaFormats.set(true);
    EXPECT_FALSE(VASurface::isSupportedFourCCThreePlaneFormat(VA_FOURCC_NV11));
    EXPECT_EQ(nullptr, VASurface::getExtendedSurfaceFormatInfo(VA_FOURCC_NV11));
}

TEST(VaSharingFunctions, givenErrorReturnedFromVaLibWhenQuerySupportedVaImageFormatsThenSupportedFormatsAreNotSet) {
    VASharingFunctionsMock sharingFunctions;
    sharingFunctions.queryImageFormatsReturnStatus = VA_STATUS_ERROR_INVALID_VALUE;

    sharingFunctions.querySupportedVaImageFormats(VADisplay(1));

    EXPECT_EQ(0u, sharingFunctions.supported2PlaneFormats.size());
    EXPECT_EQ(0u, sharingFunctions.supported3PlaneFormats.size());
}

TEST(VaSharingFunctions, givenNoSupportedFormatsWhenQuerySupportedVaImageFormatsThenSupportedFormatsAreNotSet) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(0u, sharingFunctions.supportedPackedFormats.size());
    EXPECT_EQ(0u, sharingFunctions.supported2PlaneFormats.size());
    EXPECT_EQ(0u, sharingFunctions.supported3PlaneFormats.size());
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;
    VAImageFormat vaApiFormats[10] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        10,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(0u, numImageFormats);
}

TEST(VaSharingFunctions, givenNumEntriesLowerThanSupportedFormatsWhenGettingSupportedFormatsThenOnlyNumEntiresAreReturned) {
    VASharingFunctionsMock sharingFunctions;
    VAImageFormat imageFormat = {VA_FOURCC_NV12, 1, 12};
    sharingFunctions.supported2PlaneFormats.emplace_back(imageFormat);
    imageFormat.fourcc = VA_FOURCC_P010;
    sharingFunctions.supported2PlaneFormats.emplace_back(imageFormat);

    EXPECT_EQ(2u, sharingFunctions.supported2PlaneFormats.size());

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;
    VAImageFormat vaApiFormats[3] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(2u, numImageFormats);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12), vaApiFormats[0].fourcc);
    EXPECT_EQ(0u, vaApiFormats[1].fourcc);
    EXPECT_EQ(0u, vaApiFormats[2].fourcc);
}

TEST_F(VaSharingTests, givenInteropUserSyncIsNotSpecifiedDuringContextCreationWhenEnqueueReleaseVAIsCalledThenAllWorkAlreadySubmittedShouldCompleteExecution) {
    struct MockCommandQueueToTestFinish : MockCommandQueue {
        MockCommandQueueToTestFinish(Context *context, ClDevice *device, const cl_queue_properties *props)
            : MockCommandQueue(context, device, props, false) {
        }
        cl_int finish(bool resolvePendingL3Flushes) override {
            finishCalled++;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0u;
    };

    MockContext mockContext;
    MockCommandQueueToTestFinish mockCommandQueue(&mockContext, mockContext.getDevice(0), 0);

    createMediaSurface();

    for (bool specifyInteropUseSync : {false, true}) {
        mockContext.setInteropUserSyncEnabled(specifyInteropUseSync);
        mockCommandQueue.finishCalled = 0u;

        errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(&mockCommandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);

        errCode = clEnqueueReleaseVA_APIMediaSurfacesINTEL(&mockCommandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, errCode);

        if (specifyInteropUseSync) {
            EXPECT_EQ(1u, mockCommandQueue.finishCalled);
        } else {
            EXPECT_EQ(2u, mockCommandQueue.finishCalled);
        }
    }
}

TEST_F(VaSharingTests, givenPlaneArgumentEquals2WithEmptySupported3PlaneFormatsVectorThentNoFormatIsReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 4;
    VAImageFormat vaApiFormats[4] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        2,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(0u, vaApiFormats[0].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentGreaterThan2ThenNoFormatIsReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);
    VAImageFormat imageFormat = {VA_FOURCC_RGBP, 1, 12};
    sharingFunctions.supported3PlaneFormats.emplace_back(imageFormat);
    imageFormat = {VA_FOURCC_NV12, 1, 12};
    sharingFunctions.supported2PlaneFormats.emplace_back(imageFormat);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 2;
    VAImageFormat vaApiFormats[2] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        3,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(0u, vaApiFormats[0].fourcc);
    EXPECT_EQ(0u, vaApiFormats[1].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentEquals2ThenOnlyRGBPFormatIsReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);
    VAImageFormat imageFormat = {VA_FOURCC_RGBP, 1, 12};
    sharingFunctions.supported3PlaneFormats.emplace_back(imageFormat);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 1;
    VAImageFormat vaApiFormats[3] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        2,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_RGBP), vaApiFormats[0].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentLessThan2WithProperFormatsAndEmptySupportedFormatsVectorsThenNoFormatIsReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 1;
    VAImageFormat vaApiFormats[3] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(0u, vaApiFormats[0].fourcc);

    VAImageFormat imageFormat = {VA_FOURCC_NV12, 1, 12};
    sharingFunctions.supported2PlaneFormats.emplace_back(imageFormat);
    sharingFunctions.supported3PlaneFormats.emplace_back(imageFormat);

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        1,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(0u, vaApiFormats[0].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentLessThan2WithProperFormatsAndSupportedFormatsVectorsThenAllPackedAnd2And3PlaneFormatsAreReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supportedPackedFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 6;
    VAImageFormat vaApiFormats[6] = {};

    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_NV12, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_P010, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_P016, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supported3PlaneFormats.push_back(VAImageFormat{VA_FOURCC_RGBP, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supportedPackedFormats.push_back(VAImageFormat{VA_FOURCC_YUY2, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supportedPackedFormats.push_back(VAImageFormat{VA_FOURCC_ARGB, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        6,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12), vaApiFormats[0].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_P010), vaApiFormats[1].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_P016), vaApiFormats[2].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_RGBP), vaApiFormats[3].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YUY2), vaApiFormats[4].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_ARGB), vaApiFormats[5].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentLessThan2WithPackedFormatsAndSupportedFormatsVectorsThenAllPackedFormatsAreReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supportedPackedFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 5;
    VAImageFormat vaApiFormats[5] = {};

    sharingFunctions.supportedPackedFormats.push_back(VAImageFormat{VA_FOURCC_YUY2, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        5,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_YUY2), vaApiFormats[0].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentLessThan2WithProperFormatsAndOnly3PlaneSupportedFormatsVectorThen3PlaneFormatIsReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 4;
    VAImageFormat vaApiFormats[4] = {};

    sharingFunctions.supported3PlaneFormats.push_back(VAImageFormat{VA_FOURCC_RGBP, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        4,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_RGBP), vaApiFormats[0].fourcc);
    EXPECT_EQ(0u, vaApiFormats[1].fourcc);
    EXPECT_EQ(0u, vaApiFormats[2].fourcc);
    EXPECT_EQ(0u, vaApiFormats[3].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentLessThan2WithProperFormatsAndOnly2PlaneSupportedFormatsVectorThen2PlaneFormatsAreReturned) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 4;
    VAImageFormat vaApiFormats[4] = {};

    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_NV12, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_P010, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    sharingFunctions.supported2PlaneFormats.push_back(VAImageFormat{VA_FOURCC_P016, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);

    sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        0,
        4,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12), vaApiFormats[0].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_P010), vaApiFormats[1].fourcc);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_P016), vaApiFormats[2].fourcc);
    EXPECT_EQ(0u, vaApiFormats[3].fourcc);
}

TEST_F(VaSharingTests, givenPlaneArgumentEquals2WithoutNoProperFormatsThenReturn) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(sharingFunctions.supported2PlaneFormats.size(), 0u);
    EXPECT_EQ(sharingFunctions.supported3PlaneFormats.size(), 0u);

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 1;

    sharingFunctions.supported3PlaneFormats.push_back(VAImageFormat{VA_FOURCC_RGBP, VA_LSB_FIRST, 0, 0, 0, 0, 0, 0});

    cl_int result = sharingFunctions.getSupportedFormats(
        flags,
        imageType,
        2,
        4,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(result, CL_SUCCESS);
}

class VaDeviceTests : public Test<PlatformFixture> {
  public:
    VaDeviceTests() {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    }
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
};

TEST_F(VaDeviceTests, givenVADeviceWhenGetDeviceFromVAIsCalledThenRootDeviceIsReturned) {
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    auto device = pPlatform->getClDevice(0);
    NEO::Device *neoDevice = &device->getDevice();

    auto mockDrm = static_cast<DrmMock *>(neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>());
    mockDrm->setPciPath("0000:00:02.0");

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get());
    EXPECT_NE(clDevice, nullptr);
}

TEST_F(VaDeviceTests, givenVADeviceAndInvalidPciPathOfClDeviceWhenGetDeviceFromVAIsCalledThenNullptrIsReturned) {
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    auto device = pPlatform->getClDevice(0);
    NEO::Device *neoDevice = &device->getDevice();

    auto mockDrm = static_cast<DrmMock *>(neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>());
    mockDrm->setPciPath("00:00.0");

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get());
    EXPECT_EQ(clDevice, nullptr);
}

TEST_F(VaDeviceTests, givenVADeviceAndInvalidFDWhenGetDeviceFromVAIsCalledThenNullptrIsReturned) {
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 0;

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get());
    EXPECT_EQ(clDevice, nullptr);
}

TEST_F(VaDeviceTests, givenVADeviceAndInvalidMagicNumberWhenGetDeviceFromVAIsCalledThenUnrecoverableIsCalled) {
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x0;

    VADevice vaDevice{};
    EXPECT_ANY_THROW(vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get()));
}

TEST_F(VaDeviceTests, givenVADeviceAndNegativeFdWhenGetDeviceFromVAIsCalledThenUnrecoverableIsCalled) {
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = -1;

    VADevice vaDevice{};
    EXPECT_ANY_THROW(vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get()));
}

namespace NEO {
namespace SysCalls {
extern bool makeFakeDevicePath;
extern bool allowFakeDevicePath;
} // namespace SysCalls
} // namespace NEO

TEST_F(VaDeviceTests, givenVADeviceAndFakeDevicePathWhenGetDeviceFromVAIsCalledThenNullptrIsReturned) {
    VariableBackup<bool> makeFakePathBackup(&SysCalls::makeFakeDevicePath, true);
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get());
    EXPECT_EQ(clDevice, nullptr);
}

TEST_F(VaDeviceTests, givenVADeviceAndAbsolutePathWhenGetDeviceFromVAIsCalledThenNullptrIsReturned) {
    VariableBackup<bool> makeFakePathBackup(&SysCalls::makeFakeDevicePath, true);
    VariableBackup<bool> allowFakeDevicePathBackup(&SysCalls::allowFakeDevicePath, true);
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    VADevice vaDevice{};
    auto clDevice = vaDevice.getDeviceFromVA(pPlatform, vaDisplay.get());
    EXPECT_EQ(clDevice, nullptr);
}

TEST_F(VaDeviceTests, givenValidPlatformWithInvalidVaDisplayWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnNullptrAndZeroDevices) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;
    cl_platform_id platformId = pPlatform;
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    auto device = pPlatform->getClDevice(0);
    NEO::Device *neoDevice = &device->getDevice();

    auto mockDrm = static_cast<DrmMock *>(neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>());
    mockDrm->setPciPath("00:00.0");

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(platformId, 0u, vaDisplay.get(), 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, errCode);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(nullptr, devices);
}

TEST_F(VaDeviceTests, givenValidPlatformWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnFirstDevice) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;
    cl_platform_id platformId = pPlatform;
    auto vaDisplay = std::make_unique<VADisplayContext>();
    vaDisplay->vadpy_magic = 0x56414430;
    auto contextPtr = std::make_unique<VADriverContext>();
    auto drmState = std::make_unique<int>();
    vaDisplay->pDriverContext = contextPtr.get();
    contextPtr->drm_state = drmState.get();
    *static_cast<int *>(contextPtr->drm_state) = 1;

    auto device = pPlatform->getClDevice(0);
    NEO::Device *neoDevice = &device->getDevice();

    auto mockDrm = static_cast<DrmMock *>(neoDevice->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>());
    mockDrm->setPciPath("0000:00:02.0");

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(platformId, 0u, vaDisplay.get(), 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(pPlatform->getClDevice(0), devices);
}
