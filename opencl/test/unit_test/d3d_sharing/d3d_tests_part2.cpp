/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/source/utilities/arrayref.h"
#include "opencl/source/api/api.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/d3d/d3d_buffer.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/source/sharings/d3d/d3d_texture.h"
#include "opencl/test/unit_test/fixtures/d3d_test_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace NEO {
TYPED_TEST_CASE_P(D3DTests);

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDontDoExplicitFinish) {
    this->context->setInteropUserSyncEnabled(true);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, ClDevice *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish() override {
            finishCalled++;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
    };

    auto mockCmdQ = std::unique_ptr<MockCmdQ>(new MockCmdQ(this->context, this->context->getDevice(0), 0));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdQ->finishCalled);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdQ->finishCalled);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishTwice) {
    this->context->setInteropUserSyncEnabled(false);

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, ClDevice *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish() override {
            finishCalled++;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
    };

    auto mockCmdQ = std::unique_ptr<MockCmdQ>(new MockCmdQ(this->context, this->context->getDevice(0), 0));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdQ->finishCalled);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, mockCmdQ->finishCalled);
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishOnce) {
    this->context->setInteropUserSyncEnabled(false);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, ClDevice *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish() override {
            finishCalled++;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
    };

    auto mockCmdQ = std::unique_ptr<MockCmdQ>(new MockCmdQ(this->context, this->context->getDevice(0), 0));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdQ->finishCalled);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdQ->finishCalled);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDoExplicitFinishOnce) {
    this->context->setInteropUserSyncEnabled(true);

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, ClDevice *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish() override {
            finishCalled++;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
    };

    auto mockCmdQ = std::unique_ptr<MockCmdQ>(new MockCmdQ(this->context, this->context->getDevice(0), 0));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockCmdQ->finishCalled);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, mockCmdQ.get(), 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockCmdQ->finishCalled);
}

TYPED_TEST_P(D3DTests, givenSharedResourceFlagWhenCreate2dTextureThenStagingTextureEqualsPassedTexture) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture2d(_, _, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DTexture2d *)&this->dummyD3DTexture, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);

    EXPECT_TRUE(d3dTexture->isSharedResource());
    EXPECT_EQ(&this->dummyD3DTexture, d3dTexture->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dTexture->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceFlagWhenCreate2dTextureThenCreateStagingTexture) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = 0;

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture2d(_, _, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DTexture2d *)&this->dummyD3DTextureStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DTexture2d *)&this->dummyD3DTextureStaging, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);

    EXPECT_FALSE(d3dTexture->isSharedResource());
    EXPECT_EQ(&this->dummyD3DTextureStaging, d3dTexture->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTextureStaging))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dTexture->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenSharedResourceFlagWhenCreate3dTextureThenStagingTextureEqualsPassedTexture) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture3dDesc.MipLevels = 4;

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture3d(_, _, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DTexture2d *)&this->dummyD3DTexture, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DTexture3d *)&this->dummyD3DTexture))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 0, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);

    EXPECT_TRUE(d3dTexture->isSharedResource());
    EXPECT_EQ(&this->dummyD3DTexture, d3dTexture->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dTexture->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceFlagWhenCreate3dTextureThenCreateStagingTexture) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = 0;

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture3d(_, _, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DTexture3d *)&this->dummyD3DTextureStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DTexture2d *)&this->dummyD3DTextureStaging, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DTexture3d *)&this->dummyD3DTexture))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);

    EXPECT_FALSE(d3dTexture->isSharedResource());
    EXPECT_EQ(&this->dummyD3DTextureStaging, d3dTexture->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTextureStaging))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DTexture2d *)&this->dummyD3DTexture))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dTexture->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenD3DDeviceParamWhenContextCreationThenSetProperValues) {
    cl_device_id deviceID = this->context->getDevice(0);
    cl_platform_id pid[1] = {this->pPlatform};
    auto param = this->pickParam(CL_CONTEXT_D3D10_DEVICE_KHR, CL_CONTEXT_D3D11_DEVICE_KHR);

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0], param, 0, 0};
    cl_int retVal = CL_SUCCESS;
    auto ctx = std::unique_ptr<MockContext>(Context::create<MockContext>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, ctx.get());
    EXPECT_EQ(1u, ctx->preferD3dSharedResources);
    EXPECT_NE(nullptr, ctx->getSharing<D3DSharingFunctions<TypeParam>>());
}

TYPED_TEST_P(D3DTests, givenSharedNtHandleFlagWhenCreate2dTextureThenGetNtHandle) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED_NTHANDLE;

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture2d(_, _, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DTexture2d *)&this->dummyD3DTextureStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, getSharedNTHandle(_, _))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);
}

TYPED_TEST_P(D3DTests, givenSharedNtHandleFlagWhenCreate3dTextureThenGetNtHandle) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED_NTHANDLE;

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture3d(_, _, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DTexture3d *)&this->dummyD3DTextureStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, getSharedNTHandle(_, _))
        .Times(1);

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());
    auto d3dTexture = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dTexture);
}

TYPED_TEST_P(D3DTests, WhenFillingBufferDescThenBufferContentIsCorrect) {
    D3DBufferDesc requestedDesc = {};
    D3DBufferDesc expectedDesc = {};
    expectedDesc.ByteWidth = 10;
    expectedDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->fillCreateBufferDesc(requestedDesc, 10);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3DBufferDesc)) == 0);
}

TYPED_TEST_P(D3DTests, WhenFillingTexture2dDescThenImageContentIsCorrect) {
    D3DTexture2dDesc requestedDesc = {};
    D3DTexture2dDesc expectedDesc = {};
    D3DTexture2dDesc srcDesc = {};
    cl_uint subresource = 4;

    srcDesc.Width = 10;
    srcDesc.Height = 20;
    srcDesc.MipLevels = 9;
    srcDesc.ArraySize = 5;
    srcDesc.Format = DXGI_FORMAT::DXGI_FORMAT_A8_UNORM;
    srcDesc.SampleDesc = {8, 9};
    srcDesc.BindFlags = 123;
    srcDesc.CPUAccessFlags = 456;
    srcDesc.MiscFlags = 789;

    expectedDesc.Width = srcDesc.Width;
    expectedDesc.Height = srcDesc.Height;
    expectedDesc.MipLevels = 1;
    expectedDesc.ArraySize = 1;
    expectedDesc.Format = srcDesc.Format;
    expectedDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    expectedDesc.SampleDesc = srcDesc.SampleDesc;

    for (uint32_t i = 0u; i < (subresource % srcDesc.MipLevels); i++) {
        expectedDesc.Width /= 2;
        expectedDesc.Height /= 2;
    }

    this->mockSharingFcns->fillCreateTexture2dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3DTexture2dDesc)) == 0);
}

TYPED_TEST_P(D3DTests, WhenFillingTexture3dDescThenImageContentIsCorrect) {
    D3DTexture3dDesc requestedDesc = {};
    D3DTexture3dDesc expectedDesc = {};
    D3DTexture3dDesc srcDesc = {};
    cl_uint subresource = 4;

    srcDesc.Width = 10;
    srcDesc.Height = 20;
    srcDesc.Depth = 30;
    srcDesc.MipLevels = 9;
    srcDesc.Format = DXGI_FORMAT::DXGI_FORMAT_A8_UNORM;
    srcDesc.BindFlags = 123;
    srcDesc.CPUAccessFlags = 456;
    srcDesc.MiscFlags = 789;

    expectedDesc.Width = srcDesc.Width;
    expectedDesc.Height = srcDesc.Height;
    expectedDesc.Depth = srcDesc.Depth;
    expectedDesc.MipLevels = 1;
    expectedDesc.Format = srcDesc.Format;
    expectedDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    for (uint32_t i = 0u; i < (subresource % srcDesc.MipLevels); i++) {
        expectedDesc.Width /= 2;
        expectedDesc.Height /= 2;
        expectedDesc.Depth /= 2;
    }

    this->mockSharingFcns->fillCreateTexture3dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3DTexture3dDesc)) == 0);
}

TYPED_TEST_P(D3DTests, givenPlaneWhenFindYuvSurfaceCalledThenReturnValidImgFormat) {
    const ClSurfaceFormatInfo *surfaceFormat;
    DXGI_FORMAT testFormat[] = {DXGI_FORMAT::DXGI_FORMAT_NV12, DXGI_FORMAT::DXGI_FORMAT_P010, DXGI_FORMAT::DXGI_FORMAT_P016};
    int channelDataType[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_UNORM_INT16};
    for (int n = 0; n < 3; n++) {
        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], ImagePlane::NO_PLANE, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], ImagePlane::PLANE_U, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], ImagePlane::PLANE_UV, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], ImagePlane::PLANE_V, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], ImagePlane::PLANE_Y, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_R);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);
    }
}

TYPED_TEST_P(D3DTests, GivenForced32BitAddressingWhenCreatingBufferThenBufferHas32BitAllocation) {

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());

    auto *allocation = buffer->getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_TRUE(allocation->is32BitAllocation());
}

TYPED_TEST_P(D3DTests, givenD3DTexture2dWhenOclImageIsCreatedThenSharedImageAllocationTypeIsSet) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P016;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());
    ASSERT_NE(nullptr, image->getGraphicsAllocation());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, image->getGraphicsAllocation()->getAllocationType());
}

TYPED_TEST_P(D3DTests, givenD3DTexture3dWhenOclImageIsCreatedThenSharedImageAllocationTypeIsSet) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));
    EXPECT_CALL(*this->mockSharingFcns, createTexture3d(_, _, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTextureStaging)));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create3d(this->context, reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());
    ASSERT_NE(nullptr, image->getGraphicsAllocation());
    EXPECT_EQ(GraphicsAllocation::AllocationType::SHARED_IMAGE, image->getGraphicsAllocation()->getAllocationType());
}

REGISTER_TYPED_TEST_CASE_P(D3DTests,
                           givenSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDontDoExplicitFinish,
                           givenNonSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishTwice,
                           givenSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishOnce,
                           givenNonSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDoExplicitFinishOnce,
                           givenSharedResourceFlagWhenCreate2dTextureThenStagingTextureEqualsPassedTexture,
                           givenNonSharedResourceFlagWhenCreate2dTextureThenCreateStagingTexture,
                           givenSharedResourceFlagWhenCreate3dTextureThenStagingTextureEqualsPassedTexture,
                           givenNonSharedResourceFlagWhenCreate3dTextureThenCreateStagingTexture,
                           givenD3DDeviceParamWhenContextCreationThenSetProperValues,
                           givenSharedNtHandleFlagWhenCreate2dTextureThenGetNtHandle,
                           givenSharedNtHandleFlagWhenCreate3dTextureThenGetNtHandle,
                           WhenFillingBufferDescThenBufferContentIsCorrect,
                           WhenFillingTexture2dDescThenImageContentIsCorrect,
                           WhenFillingTexture3dDescThenImageContentIsCorrect,
                           givenPlaneWhenFindYuvSurfaceCalledThenReturnValidImgFormat,
                           GivenForced32BitAddressingWhenCreatingBufferThenBufferHas32BitAllocation,
                           givenD3DTexture2dWhenOclImageIsCreatedThenSharedImageAllocationTypeIsSet,
                           givenD3DTexture3dWhenOclImageIsCreatedThenSharedImageAllocationTypeIsSet);

INSTANTIATE_TYPED_TEST_CASE_P(D3DSharingTests, D3DTests, D3DTypes);

using D3D10Test = D3DTests<D3DTypesHelper::D3D10>;

TEST_F(D3D10Test, givenIncompatibleD3DAdapterWhenGettingDeviceIdsThenNoDevicesAreReturned) {
    cl_device_id deviceID;
    cl_uint numDevices = 15;
    auto clAdapterId = context->getDevice(0)->getHardwareInfo().platform.usDeviceID;
    auto d3dAdapterId = clAdapterId + 1;
    mockSharingFcns->mockDxgiDesc.DeviceId = d3dAdapterId;

    EXPECT_NE(clAdapterId, d3dAdapterId);
    auto retVal = clGetDeviceIDsFromD3D10KHR(pPlatform, CL_D3D10_DEVICE_KHR, nullptr, CL_ALL_DEVICES_FOR_D3D10_KHR, 1, &deviceID, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(0, numDevices);
}

using D3D11Test = D3DTests<D3DTypesHelper::D3D11>;

TEST_F(D3D11Test, givenIncompatibleD3DAdapterWhenGettingDeviceIdsThenNoDevicesAreReturned) {
    cl_device_id deviceID;
    cl_uint numDevices = 15;
    auto clAdapterId = context->getDevice(0)->getHardwareInfo().platform.usDeviceID;
    auto d3dAdapterId = clAdapterId + 1;
    mockSharingFcns->mockDxgiDesc.DeviceId = d3dAdapterId;

    EXPECT_NE(clAdapterId, d3dAdapterId);
    auto retVal = clGetDeviceIDsFromD3D11KHR(pPlatform, CL_D3D11_DEVICE_KHR, nullptr, CL_ALL_DEVICES_FOR_D3D11_KHR, 1, &deviceID, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(0, numDevices);
}
} // namespace NEO
