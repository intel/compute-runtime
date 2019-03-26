/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/device/device.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/va/va_sharing.h"
#include "runtime/sharings/va/va_surface.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/sharings/va/mock_va_sharing.h"

#include "gtest/gtest.h"

using namespace NEO;

class VaSharingTests : public ::testing::Test, public PlatformFixture {
  public:
    void SetUp() override {
        PlatformFixture::SetUp();
        vaSharing = new MockVaSharing;
        context.setSharingFunctions(&vaSharing->m_sharingFunctions);
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

TEST_F(VaSharingTests, givenMockVAWhenFunctionsAreCalledThenCallsAreReceived) {
    unsigned int handle = 0u;

    EXPECT_TRUE(vaSharing->m_sharingFunctions.isValidVaDisplay());
    EXPECT_EQ(0, vaSharing->m_sharingFunctions.deriveImage(vaSurfaceId, &vaImage));
    EXPECT_EQ(0, vaSharing->m_sharingFunctions.destroyImage(vaImage.image_id));
    EXPECT_EQ(0, vaSharing->m_sharingFunctions.syncSurface(vaSurfaceId));
    EXPECT_TRUE(nullptr == vaSharing->m_sharingFunctions.getLibFunc("funcName"));
    EXPECT_EQ(0, vaSharing->m_sharingFunctions.extGetSurfaceHandle(&vaSurfaceId, &handle));

    EXPECT_EQ(sharingHandle, handle);

    EXPECT_EQ(1, vaDisplayIsValidCalled);
    EXPECT_EQ(1, vaDeriveImageCalled);
    EXPECT_EQ(1, vaDestroyImageCalled);
    EXPECT_EQ(1, vaSyncSurfaceCalled);
    EXPECT_EQ(1, vaGetLibFuncCalled);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedThenMemObjectHasVaHandler) {
    // this will create OS specific memory Manager
    //overrideCommandStreamReceiverCreation = true;
    ASSERT_FALSE(overrideCommandStreamReceiverCreation);

    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->m_sharingFunctions,
                                                      CL_MEM_READ_WRITE, &vaSurfaceId, 0, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    EXPECT_NE(nullptr, vaSurface->getGraphicsAllocation());
    EXPECT_EQ(4096u, vaSurface->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1u, vaSurface->getGraphicsAllocation()->peekSharedHandle());

    EXPECT_EQ(4096u, vaSurface->getSize());

    auto handler = vaSurface->peekSharingHandler();
    ASSERT_NE(nullptr, handler);

    auto vaHandler = static_cast<VASharing *>(handler);
    EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->m_sharingFunctions);

    EXPECT_EQ(1u, acquiredVaHandle);

    EXPECT_EQ(1, vaDeriveImageCalled);
    EXPECT_EQ(1, vaDestroyImageCalled);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);

    size_t paramSize = 0;
    void *paramValue = nullptr;
    handler->getMemObjectInfo(paramSize, paramValue);
    EXPECT_EQ(sizeof(VASurfaceID *), paramSize);
    VASurfaceID **paramSurfaceId = reinterpret_cast<VASurfaceID **>(paramValue);
    EXPECT_EQ(vaSurfaceId, **paramSurfaceId);

    delete vaSurface;
}

TEST_F(VaSharingTests, givenMockVaWhenVaSurfaceIsCreatedWithNotAlignedWidthAndHeightThenSurfaceOffsetsUseAlignedValues) {
    // this will create OS specific memory Manager
    //overrideCommandStreamReceiverCreation = true;
    ASSERT_FALSE(overrideCommandStreamReceiverCreation);
    vaSharingFunctionsMockWidth = 256 + 16;
    vaSharingFunctionsMockHeight = 512 + 16;
    auto vaSurface = VASurface::createSharedVaSurface(&context, &vaSharing->m_sharingFunctions,
                                                      CL_MEM_READ_WRITE, &vaSurfaceId, 1, &errCode);
    EXPECT_NE(nullptr, vaSurface);
    EXPECT_NE(nullptr, vaSurface->getGraphicsAllocation());
    EXPECT_EQ(4096u, vaSurface->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1u, vaSurface->getGraphicsAllocation()->peekSharedHandle());

    EXPECT_EQ(4096u, vaSurface->getSize());

    auto handler = vaSurface->peekSharingHandler();
    ASSERT_NE(nullptr, handler);

    auto vaHandler = static_cast<VASharing *>(handler);
    EXPECT_EQ(vaHandler->peekFunctionsHandler(), &vaSharing->m_sharingFunctions);

    EXPECT_EQ(1u, acquiredVaHandle);

    EXPECT_EQ(1, vaDeriveImageCalled);
    EXPECT_EQ(1, vaDestroyImageCalled);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);

    SurfaceOffsets surfaceOffsets;
    uint16_t alignedWidth = alignUp(vaSharingFunctionsMockWidth, 128);
    uint16_t alignedHeight = alignUp(vaSharingFunctionsMockHeight, 32);
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
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);

    sharedImg->peekSharingHandler()->acquire(sharedImg);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);
}

TEST_F(VaSharingTests, givenHwCommandQueueWhenAcquireIsCalledThenAcquireCountIsNotIncremented) {
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &errCode);
    ASSERT_EQ(CL_SUCCESS, errCode);

    createMediaSurface();

    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);
    errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);

    errCode = clEnqueueReleaseVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);

    errCode = clEnqueueAcquireVA_APIMediaSurfacesINTEL(commandQueue, 1, &sharedClMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, errCode);
    EXPECT_EQ(1, vaExtGetSurfaceHandleCalled);

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
    cl_uint planes[4] = {0, 1, 2, 3};
    updateAcquiredHandle(2);

    for (int i = 0; i < 4; i++) {
        createMediaSurface(planes[i]);

        EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_data_type == CL_UNORM_INT8);

        EXPECT_EQ(planes[i], sharedImg->getMediaPlaneType());
        if (planes[i] == 0u) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_R);
        } else if (planes[i] == 1) {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_RG);
        } else {
            EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_NV12_INTEL);
        }

        delete sharedImg;
        sharedImg = nullptr;
    }
}

TEST_F(VaSharingTests, givenSimpleParamsWhenCreateSurfaceIsCalledThenSetImgObject) {
    updateAcquiredHandle(2);

    createMediaSurface(3u);

    EXPECT_TRUE(sharedImg->getImageDesc().buffer == nullptr);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_array_size);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_depth);
    EXPECT_EQ(mockVaImage.height, static_cast<unsigned short>(sharedImg->getImageDesc().image_height));
    EXPECT_EQ(mockVaImage.width, static_cast<unsigned short>(sharedImg->getImageDesc().image_width));
    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == sharedImg->getImageDesc().image_type);
    EXPECT_EQ(0u, sharedImg->getImageDesc().image_slice_pitch);
    EXPECT_NE(0u, sharedImg->getImageDesc().image_row_pitch);
    EXPECT_EQ(0u, sharedImg->getHostPtrSlicePitch());
    EXPECT_NE(0u, sharedImg->getHostPtrRowPitch());
    EXPECT_TRUE(sharedImg->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(sharedImg->getCubeFaceIndex() == __GMM_NO_CUBE_MAP);

    EXPECT_EQ(vaSharing->sharingHandle, sharedImg->getGraphicsAllocation()->peekSharedHandle());
}

TEST_F(VaSharingTests, givenNonInteropUserSyncContextWhenAcquireIsCalledThenSyncSurface) {
    context.setInteropUserSyncEnabled(false);

    createMediaSurface();

    auto memObj = castToObject<MemObj>(sharedClMem);

    EXPECT_EQ(0, vaSyncSurfaceCalled);
    memObj->peekSharingHandler()->acquire(sharedImg);
    EXPECT_EQ(1, vaSyncSurfaceCalled);
}

TEST_F(VaSharingTests, givenInteropUserSyncContextWhenAcquireIsCalledThenDontSyncSurface) {
    context.setInteropUserSyncEnabled(true);

    createMediaSurface();

    EXPECT_EQ(0, vaSyncSurfaceCalled);
    sharedImg->peekSharingHandler()->acquire(sharedImg);
    EXPECT_EQ(0, vaSyncSurfaceCalled);
}

TEST_F(VaSharingTests, givenYuvPlaneWhenCreateIsCalledThenChangeWidthAndHeight) {
    cl_uint planeTypes[4] = {
        0, //Y
        1, //U
        2, //no-plane
    };

    context.setInteropUserSyncEnabled(true);

    for (int i = 0; i < 3; i++) {
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

TEST_F(VaSharingTests, givenVaSurfaceWhenCreateImageFromParentThenShareHandler) {
    context.setInteropUserSyncEnabled(true);

    createMediaSurface(2u);
    EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_NV12_INTEL);

    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 256;
    imgDesc.image_height = 256;
    imgDesc.mem_object = sharedClMem;

    cl_image_format imgFormat = {CL_R, CL_UNORM_INT8};

    auto childImg = clCreateImage(&context, CL_MEM_READ_WRITE, &imgFormat, &imgDesc, nullptr, &errCode);
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto childImgObj = castToObject<Image>(childImg);
    EXPECT_FALSE(childImgObj->getIsObjectRedescribed());

    EXPECT_EQ(sharedImg->peekSharingHandler(), childImgObj->peekSharingHandler());

    errCode = clReleaseMemObject(childImg);
    EXPECT_EQ(CL_SUCCESS, errCode);
}

TEST_F(VaSharingTests, givenVaSurfaceWhenCreateImageFromParentThenSetMediaPlaneType) {
    context.setInteropUserSyncEnabled(true);

    createMediaSurface(2u);
    EXPECT_TRUE(sharedImg->getSurfaceFormatInfo().OCLImageFormat.image_channel_order == CL_NV12_INTEL);

    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 256;
    imgDesc.image_height = 256;
    imgDesc.image_depth = 0; // Y plane
    imgDesc.mem_object = sharedClMem;

    cl_image_format imgFormat = {CL_R, CL_UNORM_INT8};

    auto childImg = clCreateImage(&context, CL_MEM_READ_WRITE, &imgFormat, &imgDesc, nullptr, &errCode);
    EXPECT_EQ(CL_SUCCESS, errCode);

    auto childImgObj = castToObject<Image>(childImg);

    EXPECT_EQ(childImgObj->getMediaPlaneType(), 0u);

    errCode = clReleaseMemObject(childImg);
    EXPECT_EQ(CL_SUCCESS, errCode);

    imgDesc.image_depth = 1; // U plane
    imgFormat.image_channel_order = CL_RG;
    childImg = clCreateImage(&context, CL_MEM_READ_WRITE, &imgFormat, &imgDesc, nullptr, &errCode);
    EXPECT_EQ(CL_SUCCESS, errCode);

    childImgObj = castToObject<Image>(childImg);

    EXPECT_EQ(childImgObj->getMediaPlaneType(), 1u);

    errCode = clReleaseMemObject(childImg);
    EXPECT_EQ(CL_SUCCESS, errCode);
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
    EXPECT_NE(nullptr, platform()->getDevice(0));
    EXPECT_EQ(platform()->getDevice(0), devices);
}

TEST_F(VaSharingTests, givenInValidPlatformWhenGetDeviceIdsFromVaApiMediaAdapterCalledThenReturnFirstDevice) {
    cl_device_id devices = 0;
    cl_uint numDevices = 0;

    auto errCode = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(nullptr, 0u, nullptr, 0u, 1, &devices, &numDevices);
    EXPECT_EQ(CL_INVALID_PLATFORM, errCode);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(0u, devices);
}
