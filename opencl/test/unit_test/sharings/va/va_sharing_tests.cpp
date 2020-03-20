/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/api/api.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/va/cl_va_api.h"
#include "opencl/source/sharings/va/va_sharing.h"
#include "opencl/source/sharings/va/va_surface.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/libult/create_command_stream.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/sharings/va/mock_va_sharing.h"

#include "gtest/gtest.h"

using namespace NEO;

class VaSharingTests : public ::testing::Test, public PlatformFixture {
  public:
    void SetUp() override {
        PlatformFixture::SetUp();
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
        PlatformFixture::TearDown();
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

    Image *sharedImg;
    cl_mem sharedClMem;
    MockContext context;
    MockVaSharing *vaSharing;
    VASurfaceID vaSurfaceId = 0u;
    VAImage vaImage = {};
    cl_int errCode;
    unsigned int sharingHandle = 1u;
};

TEST(VaSharingTest, givenVASharingFunctionsObjectWhenFunctionsAreCalledThenCallsAreRedirectedToVaFunctionPointers) {
    unsigned int handle = 0u;
    VASurfaceID vaSurfaceId = 0u;
    VAImage vaImage = {};

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
        bool vaQueryImageFormatsCalled = false;
        bool vaMaxNumImageFormatsCalled = false;

        void initMembers() {
            vaDisplayIsValidPFN = mockVaDisplayIsValid;
            vaDeriveImagePFN = mockVaDeriveImage;
            vaDestroyImagePFN = mockVaDestroyImage;
            vaSyncSurfacePFN = mockVaSyncSurface;
            vaGetLibFuncPFN = mockVaGetLibFunc;
            vaExtGetSurfaceHandlePFN = mockExtGetSurfaceHandle;
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
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaQueryImageFormatsCalled);
    EXPECT_TRUE(VASharingFunctionsGlobalFunctionPointersMock::getInstance(false)->vaMaxNumImageFormatsCalled);

    VASharingFunctionsGlobalFunctionPointersMock::getInstance(true);
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedThenMemObjectHasVaHandler) {
    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                      CL_MEM_READ_WRITE, &vaSurfaceId, 0, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    EXPECT_NE(nullptr, vaSurface->getGraphicsAllocation());
    EXPECT_EQ(4096u, vaSurface->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1u, vaSurface->getGraphicsAllocation()->peekSharedHandle());

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

TEST_F(VaSharingTests, givenInvalidPlaneWhenVaSurfaceIsCreatedThenUnrecoverableIsCalled) {
    EXPECT_THROW(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                  CL_MEM_READ_WRITE, &vaSurfaceId, 2, &errCode),
                 std::exception);
}

TEST_F(VaSharingTests, givenInvalidPlaneInputWhenVaSurfaceIsCreatedThenInvalidValueErrorIsReturned) {
    sharedClMem = clCreateFromVA_APIMediaSurfaceINTEL(&context, CL_MEM_READ_WRITE, &vaSurfaceId, 2, &errCode);
    EXPECT_EQ(nullptr, sharedClMem);
    EXPECT_EQ(CL_INVALID_VALUE, errCode);
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedWithNotAlignedWidthAndHeightThenSurfaceOffsetsUseAlignedValues) {
    vaSharing->sharingFunctions.derivedImageWidth = 256 + 16;
    vaSharing->sharingFunctions.derivedImageHeight = 512 + 16;
    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                      CL_MEM_READ_WRITE, &vaSurfaceId, 1, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    EXPECT_NE(nullptr, vaSurface->getGraphicsAllocation());
    EXPECT_EQ(4096u, vaSurface->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1u, vaSurface->getGraphicsAllocation()->peekSharedHandle());

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

    sharedImg->peekSharingHandler()->acquire(sharedImg);
    EXPECT_TRUE(vaSharing->sharingFunctions.extGetSurfaceHandleCalled);

    vaSharing->sharingFunctions.extGetSurfaceHandleCalled = false;
    sharedImg->peekSharingHandler()->acquire(sharedImg);
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
    createMediaSurface();

    VASurfaceID *retVaSurfaceId = nullptr;
    size_t retSize = 0;
    errCode = clGetMemObjectInfo(sharedClMem, CL_MEM_VA_API_MEDIA_SURFACE_INTEL,
                                 sizeof(VASurfaceID *), &retVaSurfaceId, &retSize);

    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(sizeof(VASurfaceID *), retSize);
    EXPECT_EQ(vaSurfaceId, *retVaSurfaceId);
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

        EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type == CL_UNORM_INT8);

        EXPECT_EQ(planes[i], sharedImg->getMediaPlaneType());
        if (planes[i] == 0u) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_R);
        } else if (planes[i] == 1) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_RG);
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
    EXPECT_TRUE(sharedImg->getMemoryPropertiesFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(sharedImg->getCubeFaceIndex() == __GMM_NO_CUBE_MAP);

    EXPECT_EQ(vaSharing->sharingHandle, sharedImg->getGraphicsAllocation()->peekSharedHandle());
}

TEST_F(VaSharingTests, givenNonInteropUserSyncContextWhenAcquireIsCalledThenSyncSurface) {
    context.setInteropUserSyncEnabled(false);

    createMediaSurface();

    auto memObj = castToObject<MemObj>(sharedClMem);

    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
    memObj->peekSharingHandler()->acquire(sharedImg);
    EXPECT_TRUE(vaSharing->sharingFunctions.syncSurfaceCalled);
}

TEST_F(VaSharingTests, givenInteropUserSyncContextWhenAcquireIsCalledThenDontSyncSurface) {
    context.setInteropUserSyncEnabled(true);

    createMediaSurface();

    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
    sharedImg->peekSharingHandler()->acquire(sharedImg);
    EXPECT_FALSE(vaSharing->sharingFunctions.syncSurfaceCalled);
}

TEST_F(VaSharingTests, givenYuvPlaneWhenCreateIsCalledThenChangeWidthAndHeight) {
    cl_uint planeTypes[] = {
        0, //Y
        1  //U
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

TEST_F(VaSharingTests, givenValidPlatformWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnFirstDevice) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;

    cl_platform_id platformId = this->pPlatform;

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(platformId, 0u, nullptr, 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(1u, numDevices);
    EXPECT_NE(nullptr, platform()->getClDevice(0));
    EXPECT_EQ(platform()->getClDevice(0), devices);
}

TEST_F(VaSharingTests, givenInValidPlatformWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnFirstDevice) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(nullptr, 0u, nullptr, 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_INVALID_PLATFORM, errCode);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(0u, devices);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndP010FormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P010;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, &vaSurfaceId, 0, &errCode));
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.GMMSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P010, vaSurface->getGraphicsAllocation()->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndP010FormatWhenCreatingSharedVaSurfaceForPlane1ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.derivedImageFormatBpp = 16;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_P010;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, &vaSurfaceId, 1, &errCode));
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT16), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_RG), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R16G16_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.GMMSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_P010, vaSurface->getGraphicsAllocation()->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenEnabledExtendedVaFormatsAndNV12FormatWhenCreatingSharedVaSurfaceForPlane0ThenCorrectFormatIsUsedByImageAndGMM) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableExtendedVaFormats.set(true);

    vaSharing->sharingFunctions.derivedImageFormatBpp = 12;
    vaSharing->sharingFunctions.derivedImageFormatFourCC = VA_FOURCC_NV12;

    auto vaSurface = std::unique_ptr<Image>(VASurface::createSharedVaSurface(&context, &vaSharing->sharingFunctions,
                                                                             CL_MEM_READ_WRITE, &vaSurfaceId, 0, &errCode));
    EXPECT_EQ(static_cast<cl_channel_type>(CL_UNORM_INT8), vaSurface->getImageFormat().image_channel_data_type);
    EXPECT_EQ(static_cast<cl_channel_order>(CL_R), vaSurface->getImageFormat().image_channel_order);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8_UNORM, vaSurface->getSurfaceFormatInfo().surfaceFormat.GMMSurfaceFormat);
    EXPECT_EQ(GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12, vaSurface->getGraphicsAllocation()->getDefaultGmm()->resourceParams.Format);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

using ApiVaSharingTests = VaSharingTests;

TEST_F(ApiVaSharingTests, givenSupportedImageTypeWhenGettingSupportedVAApiFormatsThenCorrectListIsReturned) {
    cl_mem_flags flags[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE};
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    VAImageFormat supportedFormat = {VA_FOURCC_NV12, VA_LSB_FIRST, 8, 0, 0, 0, 0, 0};

    for (auto flag : flags) {

        cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
            &context,
            flag,
            image_type,
            arrayCount(vaApiFormats),
            vaApiFormats,
            &numImageFormats);

        EXPECT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(1u, numImageFormats);

        EXPECT_EQ(supportedFormat.fourcc, vaApiFormats[0].fourcc);
    }
}

TEST_F(ApiVaSharingTests, givenZeroNumEntriesWhenGettingSupportedVAApiFormatsThenNumFormatsIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        image_type,
        0,
        nullptr,
        &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(1u, numImageFormats);
}

TEST_F(ApiVaSharingTests, givenNullNumImageFormatsWhenGettingSupportedVAApiFormatsThenNumFormatsIsNotDereferenced) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        image_type,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, result);
}

TEST_F(ApiVaSharingTests, givenInvalidImageTypeWhenGettingSupportedVAApiFormatsThenIvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE3D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        image_type,
        arrayCount(vaApiFormats),
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(ApiVaSharingTests, givenInvalidFlagsWhenGettingSupportedVAApiFormatsThenIvalidValueErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_NO_ACCESS_INTEL;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &context,
        flags,
        image_type,
        arrayCount(vaApiFormats),
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(ApiVaSharingTests, givenInvalidContextWhenGettingSupportedVAApiFormatsThenIvalidContextErrorIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    VAImageFormat vaApiFormats[10] = {};
    cl_uint numImageFormats = 0;

    MockContext contextWihtoutVASharing;
    cl_int result = clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
        &contextWihtoutVASharing,
        flags,
        image_type,
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
    }
}

TEST(VaSurface, givenInValidPlaneOrFlagsWhenValidatingInputsThenTrueIsReturned) {
    cl_uint plane = 2;
    EXPECT_FALSE(VASurface::validate(CL_MEM_READ_ONLY, plane));
    EXPECT_FALSE(VASurface::validate(CL_MEM_USE_HOST_PTR, 0));
}

TEST(VaSurface, givenEnabledExtendedVaFormatsWhenGettingUnsupportedSurfaceFormatInfoThenNullptrIsReturned) {
    auto formatInfo = VASurface::getExtendedSurfaceFormatInfo(VA_FOURCC_P016);
    EXPECT_EQ(nullptr, formatInfo);
}

TEST(VaSurface, givenNotSupportedVaFormatsWhenCheckingIfSupportedThenFalseIsReturned) {
    EXPECT_FALSE(VASurface::isSupportedFourCC(VA_FOURCC_NV11));

    DebugManagerStateRestore restore;
    DebugManager.flags.EnableExtendedVaFormats.set(true);
    EXPECT_FALSE(VASurface::isSupportedFourCC(VA_FOURCC_P016));
}

TEST(VaSharingFunctions, givenErrorReturnedFromVaLibWhenQuerySupportedVaImageFormatsThenSupportedFormatsAreNotSet) {
    VASharingFunctionsMock sharingFunctions;
    sharingFunctions.queryImageFormatsReturnStatus = VA_STATUS_ERROR_INVALID_VALUE;

    sharingFunctions.querySupportedVaImageFormats(VADisplay(1));

    EXPECT_EQ(0u, sharingFunctions.supportedFormats.size());
}

TEST(VaSharingFunctions, givenNoSupportedFormatsWhenQuerySupportedVaImageFormatsThenSupportedFormatsAreNotSet) {
    VASharingFunctionsMock sharingFunctions;
    EXPECT_EQ(0u, sharingFunctions.supportedFormats.size());

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;
    VAImageFormat vaApiFormats[10] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        image_type,
        10,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(0u, numImageFormats);
}

TEST(VaSharingFunctions, givenNumEntriesLowerThanSupportedFormatsWhenGettingSupportedFormatsThenOnlyNumEntiresAreReturned) {
    VASharingFunctionsMock sharingFunctions;
    VAImageFormat imageFormat = {VA_FOURCC_NV12, 1, 12};
    sharingFunctions.supportedFormats.emplace_back(imageFormat);
    imageFormat.fourcc = VA_FOURCC_NV21;
    sharingFunctions.supportedFormats.emplace_back(imageFormat);

    EXPECT_EQ(2u, sharingFunctions.supportedFormats.size());

    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;
    VAImageFormat vaApiFormats[3] = {};

    sharingFunctions.getSupportedFormats(
        flags,
        image_type,
        1,
        vaApiFormats,
        &numImageFormats);

    EXPECT_EQ(2u, numImageFormats);
    EXPECT_EQ(static_cast<uint32_t>(VA_FOURCC_NV12), vaApiFormats[0].fourcc);
    EXPECT_EQ(0u, vaApiFormats[1].fourcc);
    EXPECT_EQ(0u, vaApiFormats[2].fourcc);
}
