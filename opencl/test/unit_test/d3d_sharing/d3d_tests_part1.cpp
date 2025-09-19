/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/driver_info_windows.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"

#include "opencl/source/api/api.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/d3d/d3d_buffer.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/source/sharings/d3d/d3d_texture.h"
#include "opencl/source/sharings/d3d/enable_d3d.h"
#include "opencl/test/unit_test/fixtures/d3d_test_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_sharing_factory.h"

#include "gtest/gtest.h"

namespace NEO {
TYPED_TEST_SUITE_P(D3DTests);

TYPED_TEST_P(D3DTests, GivenSpecificDeviceSetWhenGettingDeviceIDsFromD3DThenOnlySelectedDevicesAreReturned) {
    cl_device_id expectedDevice = *this->devices;
    cl_device_id device = 0;
    cl_uint numDevices = 0;

    auto deviceSourceParam = this->pickParam(CL_D3D10_DEVICE_KHR, CL_D3D11_DEVICE_KHR);
    auto deviceSetParam = this->pickParam(CL_PREFERRED_DEVICES_FOR_D3D10_KHR, CL_PREFERRED_DEVICES_FOR_D3D11_KHR);

    cl_int retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, &this->dummyD3DBuffer,
                                                 deviceSetParam, 0, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);

    device = 0;
    numDevices = 0;
    deviceSetParam = this->pickParam(CL_ALL_DEVICES_FOR_D3D10_KHR, CL_ALL_DEVICES_FOR_D3D11_KHR);
    retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, &this->dummyD3DBuffer,
                                          deviceSetParam, 0, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);

    device = 0;
    numDevices = 0;
    retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, &this->dummyD3DBuffer,
                                          CL_INVALID_OPERATION, 0, &device, &numDevices);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_NE(expectedDevice, device);
    EXPECT_EQ(0u, numDevices);
}

TYPED_TEST_P(D3DTests, GivenSpecificDeviceSourceWhenGettingDeviceIDsFromD3DThenOnlySelectedDevicesAreReturned) {
    cl_device_id expectedDevice = *this->devices;
    cl_device_id device = 0;
    cl_uint numDevices = 0;

    auto deviceSourceParam = this->pickParam(CL_D3D10_DEVICE_KHR, CL_D3D11_DEVICE_KHR);
    auto deviceSetParam = this->pickParam(CL_ALL_DEVICES_FOR_D3D10_KHR, CL_ALL_DEVICES_FOR_D3D11_KHR);

    cl_int retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, &this->dummyD3DBuffer,
                                                 deviceSetParam, 0, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(1u, this->mockSharingFcns->getDxgiDescCalled);
    EXPECT_EQ(nullptr, this->mockSharingFcns->getDxgiDescAdapterRequested);

    device = 0;
    numDevices = 0;
    deviceSourceParam = this->pickParam(CL_D3D10_DXGI_ADAPTER_KHR, CL_D3D11_DXGI_ADAPTER_KHR);
    retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, &this->dummyD3DBuffer,
                                          deviceSetParam, 0, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(2u, this->mockSharingFcns->getDxgiDescCalled);
    EXPECT_NE(nullptr, this->mockSharingFcns->getDxgiDescAdapterRequested);

    device = 0;
    numDevices = 0;
    retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, CL_INVALID_OPERATION, &this->dummyD3DBuffer,
                                          deviceSetParam, 0, &device, &numDevices);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_NE(expectedDevice, device);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(2u, this->mockSharingFcns->getDxgiDescCalled);
}

TYPED_TEST_P(D3DTests, givenNonIntelVendorWhenGetDeviceIdIsCalledThenReturnError) {
    DXGI_ADAPTER_DESC desc = {{0}};
    desc.VendorId = INTEL_VENDOR_ID + 1u;
    this->mockSharingFcns->mockDxgiDesc = desc;

    cl_device_id device = 0;
    cl_uint numDevices = 0;

    auto deviceSourceParam = this->pickParam(CL_D3D10_DEVICE_KHR, CL_D3D11_DEVICE_KHR);
    auto deviceSetParam = this->pickParam(CL_ALL_DEVICES_FOR_D3D10_KHR, CL_ALL_DEVICES_FOR_D3D11_KHR);

    cl_int retVal = this->getDeviceIDsFromD3DApi(this->mockSharingFcns, this->pPlatform, deviceSourceParam, nullptr,
                                                 deviceSetParam, 0, &device, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_TRUE(0 == device);
    EXPECT_EQ(0u, numDevices);
    EXPECT_EQ(1u, this->mockSharingFcns->getDxgiDescCalled);
}

TYPED_TEST_P(D3DTests, WhenCreatingFromD3DBufferKhrApiThenValidBufferIsReturned) {
    cl_int retVal;

    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto buffer = castToObject<Buffer>(memObj);
    ASSERT_NE(nullptr, buffer);
    ASSERT_NE(nullptr, buffer->getSharingHandler().get());

    auto bufferObj = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());

    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DResource *>(&this->dummyD3DBuffer), *bufferObj->getResourceHandler());
    EXPECT_TRUE(buffer->getFlags() == CL_MEM_READ_WRITE);

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->getSharedNTHandleCalled);
}

TYPED_TEST_P(D3DTests, givenNV12FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());
    EXPECT_EQ(GMM_PLANE_Y, image->getPlane());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_NV12, ImagePlane::planeY, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenSharedObjectFromInvalidContextWhen2dCreatedThenReturnCorrectCode) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    cl_int retCode = 0;
    this->mockMM->verifyValue = false;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, &retCode));
    this->mockMM->verifyValue = true;
    EXPECT_EQ(nullptr, image.get());
    EXPECT_EQ(retCode, CL_INVALID_D3D11_RESOURCE_KHR);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenSharedObjectFromInvalidContextAndNTHandleWhen2dCreatedThenReturnCorrectCode) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED_NTHANDLE;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    cl_int retCode = 0;
    this->mockMM->verifyValue = false;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, &retCode));
    this->mockMM->verifyValue = true;
    EXPECT_EQ(nullptr, image.get());
    EXPECT_EQ(retCode, CL_INVALID_D3D11_RESOURCE_KHR);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenSharedObjectAndAllocationFailedWhen2dCreatedThenReturnCorrectCode) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    cl_int retCode = 0;
    this->mockMM->failAlloc = true;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, &retCode));
    this->mockMM->failAlloc = false;
    EXPECT_EQ(nullptr, image.get());
    EXPECT_EQ(retCode, CL_OUT_OF_HOST_MEMORY);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenSharedObjectAndNTHandleAndAllocationFailedWhen2dCreatedThenReturnCorrectCode) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED_NTHANDLE;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    cl_int retCode = 0;
    this->mockMM->failAlloc = true;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, &retCode));
    this->mockMM->failAlloc = false;
    EXPECT_EQ(nullptr, image.get());
    EXPECT_EQ(retCode, CL_OUT_OF_HOST_MEMORY);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenNV12FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());
    EXPECT_EQ(GMM_PLANE_U, image->getPlane());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_NV12, ImagePlane::planeUV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenP010FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P010;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P010, ImagePlane::planeY, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenP010FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P010;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P010, ImagePlane::planeUV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenP016FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P016;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P016, ImagePlane::planeY, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenP016FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P016;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P016, ImagePlane::planeUV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, WhenCreatingFromD3D2dTextureKhrApiThenValidImageIsReturned) {
    cl_int retVal;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), 1, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto image = castToObject<Image>(memObj);
    ASSERT_NE(nullptr, image);
    ASSERT_NE(nullptr, image->getSharingHandler().get());

    auto textureObj = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());

    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DResource *>(&this->dummyD3DTexture), *textureObj->getResourceHandler());
    EXPECT_TRUE(image->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D);
    EXPECT_EQ(1u, textureObj->getSubresource());

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, this->mockSharingFcns->createQueryCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->getSharedNTHandleCalled);
}

TYPED_TEST_P(D3DTests, WhenCreatingFromD3D3dTextureKhrApiThenValidImageIsReturned) {
    cl_int retVal;

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    auto memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture3d *>(&this->dummyD3DTexture), 1, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, this->mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, this->mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto image = castToObject<Image>(memObj);
    ASSERT_NE(nullptr, image);
    ASSERT_NE(nullptr, image->getSharingHandler().get());

    auto textureObj = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());

    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DResource *>(&this->dummyD3DTexture), *textureObj->getResourceHandler());
    EXPECT_TRUE(image->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE3D);
    EXPECT_EQ(1u, textureObj->getSubresource());

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, this->mockSharingFcns->createQueryCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->getSharedNTHandleCalled);
}

TYPED_TEST_P(D3DTests, givenSharedResourceFlagWhenCreateBufferThenStagingBufferEqualsPassedBuffer) {
    std::vector<IUnknown *> releaseExpectedParams{};

    {
        this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

        this->mockSharingFcns->getBufferDescSetParams = true;
        this->mockSharingFcns->getBufferDescParamsSet.bufferDesc = this->mockSharingFcns->mockBufferDesc;

        this->mockSharingFcns->createQuerySetParams = true;
        this->mockSharingFcns->createQueryParamsSet.query = reinterpret_cast<typename TestFixture::D3DQuery *>(0x8);

        auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), CL_MEM_READ_WRITE, nullptr));
        ASSERT_NE(nullptr, buffer.get());
        auto d3dBuffer = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());
        ASSERT_NE(nullptr, d3dBuffer);

        EXPECT_NE(nullptr, d3dBuffer->getQuery());
        EXPECT_TRUE(d3dBuffer->isSharedResource());
        EXPECT_EQ(&this->dummyD3DBuffer, d3dBuffer->getResourceStaging());

        releaseExpectedParams.push_back(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer));
        releaseExpectedParams.push_back(reinterpret_cast<typename TestFixture::D3DQuery *>(d3dBuffer->getQuery()));

        EXPECT_EQ(0u, this->mockSharingFcns->createBufferCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->createQueryCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->getBufferDescCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->addRefCalled);

        EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), this->mockSharingFcns->getSharedHandleParamsPassed[0].resource);
        EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), this->mockSharingFcns->addRefParamsPassed[0].resource);
    }
    EXPECT_EQ(2u, this->mockSharingFcns->releaseCalled);

    EXPECT_EQ(releaseExpectedParams[0], this->mockSharingFcns->releaseParamsPassed[0].resource);
    EXPECT_EQ(releaseExpectedParams[1], this->mockSharingFcns->releaseParamsPassed[1].resource);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceFlagWhenCreateBufferThenCreateNewStagingBuffer) {
    std::vector<IUnknown *> releaseExpectedParams{};

    {
        this->mockSharingFcns->createBufferSetParams = true;
        this->mockSharingFcns->createBufferParamsSet.buffer = reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging);

        this->mockSharingFcns->createQuerySetParams = true;
        this->mockSharingFcns->createQueryParamsSet.query = reinterpret_cast<typename TestFixture::D3DQuery *>(0x8);

        auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), CL_MEM_READ_WRITE, nullptr));
        ASSERT_NE(nullptr, buffer.get());
        auto d3dBuffer = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());
        ASSERT_NE(nullptr, d3dBuffer);

        EXPECT_NE(nullptr, d3dBuffer->getQuery());
        EXPECT_FALSE(d3dBuffer->isSharedResource());
        EXPECT_EQ(&this->dummyD3DBufferStaging, d3dBuffer->getResourceStaging());

        releaseExpectedParams.push_back(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging));
        releaseExpectedParams.push_back(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer));
        releaseExpectedParams.push_back(reinterpret_cast<typename TestFixture::D3DQuery *>(d3dBuffer->getQuery()));

        EXPECT_EQ(1u, this->mockSharingFcns->createBufferCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->createQueryCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
        EXPECT_EQ(1u, this->mockSharingFcns->addRefCalled);

        EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging), this->mockSharingFcns->getSharedHandleParamsPassed[0].resource);
        EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), this->mockSharingFcns->addRefParamsPassed[0].resource);
    }
    EXPECT_EQ(3u, this->mockSharingFcns->releaseCalled);

    EXPECT_EQ(releaseExpectedParams[0], this->mockSharingFcns->releaseParamsPassed[0].resource);
    EXPECT_EQ(releaseExpectedParams[1], this->mockSharingFcns->releaseParamsPassed[1].resource);
    EXPECT_EQ(releaseExpectedParams[2], this->mockSharingFcns->releaseParamsPassed[2].resource);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceBufferWhenAcquiredThenCopySubregion) {
    this->context->setInteropUserSyncEnabled(true);

    this->mockSharingFcns->createBufferSetParams = true;
    this->mockSharingFcns->createBufferParamsSet.buffer = reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging);

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    // acquireCount == 0, acquire
    EXPECT_EQ(0u, buffer->acquireCount);
    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, buffer->acquireCount);

    // acquireCount == 1, don't acquire
    retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(this->pickParam(CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR, CL_D3D11_RESOURCE_ALREADY_ACQUIRED_KHR), retVal);
    EXPECT_EQ(1u, buffer->acquireCount);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    // acquireCount == 0
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, buffer->acquireCount);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(this->pickParam(CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR, CL_D3D11_RESOURCE_NOT_ACQUIRED_KHR), retVal);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1u, this->mockSharingFcns->createBufferCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(2u, this->mockSharingFcns->getDeviceContextCalled);
    EXPECT_EQ(2u, this->mockSharingFcns->releaseDeviceContextCalled);
    EXPECT_EQ(2u, this->mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->flushAndWaitCalled);

    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging), this->mockSharingFcns->copySubresourceRegionParamsPassed[0].dst);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionParamsPassed[0].dstSubresource);
    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), this->mockSharingFcns->copySubresourceRegionParamsPassed[0].src);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionParamsPassed[0].srcSubresource);

    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), this->mockSharingFcns->copySubresourceRegionParamsPassed[1].dst);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionParamsPassed[1].dstSubresource);
    EXPECT_EQ(reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBufferStaging), this->mockSharingFcns->copySubresourceRegionParamsPassed[1].src);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionParamsPassed[1].srcSubresource);
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferWhenAcquiredThenDontCopySubregion) {
    this->context->setInteropUserSyncEnabled(true);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->getBufferDescSetParams = true;
    this->mockSharingFcns->getBufferDescParamsSet.bufferDesc = this->mockSharingFcns->mockBufferDesc;

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getBufferDescCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getDeviceContextCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->releaseDeviceContextCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->flushAndWaitCalled);
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncDisabledWhenAcquiredThenFlushOnAcquire) {
    this->context->setInteropUserSyncEnabled(false);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->getBufferDescSetParams = true;
    this->mockSharingFcns->getBufferDescParamsSet.bufferDesc = this->mockSharingFcns->mockBufferDesc;

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getBufferDescCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getSharedHandleCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->getDeviceContextCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->releaseDeviceContextCalled);
    EXPECT_EQ(0u, this->mockSharingFcns->copySubresourceRegionCalled);
    EXPECT_EQ(1u, this->mockSharingFcns->flushAndWaitCalled);
}

TYPED_TEST_P(D3DTests, WhenGettingPreferD3DSharedResourcesThenCorrectValueIsReturned) {
    auto ctx = std::unique_ptr<MockContext>(new MockContext());
    cl_bool retBool = 0;
    size_t size = 0;
    auto param = this->pickParam(CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR, CL_CONTEXT_D3D11_PREFER_SHARED_RESOURCES_KHR);

    ctx->preferD3dSharedResources = 1u;
    auto retVal = ctx->getInfo(param, sizeof(retBool), &retBool, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_EQ(1u, retBool);

    ctx->preferD3dSharedResources = 0u;
    retVal = ctx->getInfo(param, sizeof(retBool), &retBool, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_EQ(0u, retBool);
}

TYPED_TEST_P(D3DTests, WhenGettingD3DResourceInfoFromMemObjThenCorrectInfoIsReturned) {
    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), nullptr);
    ASSERT_NE(nullptr, memObj);
    auto param = this->pickParam(CL_MEM_D3D10_RESOURCE_KHR, CL_MEM_D3D11_RESOURCE_KHR);

    void *retBuffer = nullptr;
    size_t retSize = 0;
    clGetMemObjectInfo(memObj, param, sizeof(typename TestFixture::D3DBufferObj), &retBuffer, &retSize);
    EXPECT_EQ(sizeof(typename TestFixture::D3DBufferObj), retSize);
    EXPECT_EQ(&this->dummyD3DBuffer, retBuffer);

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, WhenGettingD3DSubresourceInfoFromMemObjThenCorrectInfoIsReturned) {
    cl_int retVal;
    cl_uint subresource = 1u;
    auto param = this->pickParam(CL_IMAGE_D3D10_SUBRESOURCE_KHR, CL_IMAGE_D3D11_SUBRESOURCE_KHR);

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint retSubresource = 0;
    size_t retSize = 0;
    clGetImageInfo(memObj, param, sizeof(cl_uint), &retSubresource, &retSize);
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(subresource, retSubresource);

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenTheSameD3DBufferWhenNextCreateIsCalledThenFail) {
    cl_int retVal;

    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->at(0).second);
    auto memObj2 = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DBufferObj *>(&this->dummyD3DBuffer), &retVal);
    EXPECT_EQ(nullptr, memObj2);
    EXPECT_EQ(this->pickParam(CL_INVALID_D3D10_RESOURCE_KHR, CL_INVALID_D3D11_RESOURCE_KHR), retVal);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    clReleaseMemObject(memObj);
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
}

TYPED_TEST_P(D3DTests, givenD3DTextureWithTheSameSubresourceWhenNextCreateIsCalledThenFail) {
    cl_int retVal;
    cl_uint subresource = 1;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj2 = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    EXPECT_EQ(nullptr, memObj2);
    EXPECT_EQ(this->pickParam(CL_INVALID_D3D10_RESOURCE_KHR, CL_INVALID_D3D11_RESOURCE_KHR), retVal);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);

    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    subresource++;
    this->setupMockGmm(); // setup new mock for new resource
    auto memObj3 = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    clReleaseMemObject(memObj);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    clReleaseMemObject(memObj3);
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    EXPECT_EQ(2u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenInvalidSubresourceWhenCreateTexture2dIsCalledThenFail) {
    cl_int retVal;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;
    cl_uint subresource = 16;

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);

    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    subresource = 20;
    memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture2d *>(&this->dummyD3DTexture), subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(2u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DTests, givenInvalidSubresourceWhenCreateTexture3dIsCalledThenFail) {
    cl_int retVal;
    this->mockSharingFcns->mockTexture3dDesc.MipLevels = 4;
    cl_uint subresource = 16;

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    auto memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture3d *>(&this->dummyD3DTexture), subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);

    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    subresource = 20;
    memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, reinterpret_cast<typename TestFixture::D3DTexture3d *>(&this->dummyD3DTexture), subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_EQ(2u, this->mockSharingFcns->getTexture3dDescCalled);
}

TYPED_TEST_P(D3DTests, givenPackedFormatWhenLookingForSurfaceFormatWithPackedNotSupportedThenReturnNull) {
    EXPECT_GT(SurfaceFormats::packed().size(), 0u);
    for (auto &format : SurfaceFormats::packed()) {
        auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.surfaceFormat.gmmSurfaceFormat, CL_MEM_READ_ONLY, false /* supportsOcl20Features */, false /* packedSupported */);
        ASSERT_EQ(nullptr, surfaceFormat);
    }
}

TYPED_TEST_P(D3DTests, givenPackedFormatWhenLookingForSurfaceFormatWithPackedSupportedThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::packed().size(), 0u);
    uint32_t counter = 0;
    for (auto &format : SurfaceFormats::packed()) {
        auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.surfaceFormat.gmmSurfaceFormat, CL_MEM_READ_ONLY, false /* supportsOcl20Features */, true /* packedSupported */);
        ASSERT_NE(nullptr, surfaceFormat);
        counter++;
        EXPECT_EQ(&format, surfaceFormat);
    }
    EXPECT_NE(counter, 0U);
}

TYPED_TEST_P(D3DTests, givenReadonlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::readOnly12().size(), 0u);
    for (auto &format : SurfaceFormats::readOnly12()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.oclImageFormat.image_channel_order == CL_RGBA ||
            format.oclImageFormat.image_channel_order == CL_BGRA ||
            format.oclImageFormat.image_channel_order == CL_RG ||
            format.oclImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.surfaceFormat.gmmSurfaceFormat, CL_MEM_READ_ONLY, false /* supportsOcl20Features */, true);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

TYPED_TEST_P(D3DTests, givenWriteOnlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::writeOnly().size(), 0u);
    for (auto &format : SurfaceFormats::writeOnly()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.oclImageFormat.image_channel_order == CL_RGBA ||
            format.oclImageFormat.image_channel_order == CL_BGRA ||
            format.oclImageFormat.image_channel_order == CL_RG ||
            format.oclImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.surfaceFormat.gmmSurfaceFormat, CL_MEM_WRITE_ONLY, this->context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features, true);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

TYPED_TEST_P(D3DTests, givenReadWriteFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::readWrite().size(), 0u);
    for (auto &format : SurfaceFormats::readWrite()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.oclImageFormat.image_channel_order == CL_RGBA ||
            format.oclImageFormat.image_channel_order == CL_BGRA ||
            format.oclImageFormat.image_channel_order == CL_RG ||
            format.oclImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.surfaceFormat.gmmSurfaceFormat, CL_MEM_READ_WRITE, this->context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features, true);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

REGISTER_TYPED_TEST_SUITE_P(D3DTests,
                            GivenSpecificDeviceSetWhenGettingDeviceIDsFromD3DThenOnlySelectedDevicesAreReturned,
                            GivenSpecificDeviceSourceWhenGettingDeviceIDsFromD3DThenOnlySelectedDevicesAreReturned,
                            givenNonIntelVendorWhenGetDeviceIdIsCalledThenReturnError,
                            WhenCreatingFromD3DBufferKhrApiThenValidBufferIsReturned,
                            WhenCreatingFromD3D2dTextureKhrApiThenValidImageIsReturned,
                            WhenCreatingFromD3D3dTextureKhrApiThenValidImageIsReturned,
                            givenSharedResourceFlagWhenCreateBufferThenStagingBufferEqualsPassedBuffer,
                            givenNonSharedResourceFlagWhenCreateBufferThenCreateNewStagingBuffer,
                            givenNonSharedResourceBufferWhenAcquiredThenCopySubregion,
                            givenSharedResourceBufferWhenAcquiredThenDontCopySubregion,
                            givenSharedResourceBufferAndInteropUserSyncDisabledWhenAcquiredThenFlushOnAcquire,
                            WhenGettingPreferD3DSharedResourcesThenCorrectValueIsReturned,
                            WhenGettingD3DResourceInfoFromMemObjThenCorrectInfoIsReturned,
                            WhenGettingD3DSubresourceInfoFromMemObjThenCorrectInfoIsReturned,
                            givenTheSameD3DBufferWhenNextCreateIsCalledThenFail,
                            givenD3DTextureWithTheSameSubresourceWhenNextCreateIsCalledThenFail,
                            givenInvalidSubresourceWhenCreateTexture2dIsCalledThenFail,
                            givenInvalidSubresourceWhenCreateTexture3dIsCalledThenFail,
                            givenReadonlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                            givenWriteOnlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                            givenReadWriteFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                            givenNV12FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                            givenSharedObjectFromInvalidContextWhen2dCreatedThenReturnCorrectCode,
                            givenSharedObjectFromInvalidContextAndNTHandleWhen2dCreatedThenReturnCorrectCode,
                            givenSharedObjectAndAllocationFailedWhen2dCreatedThenReturnCorrectCode,
                            givenSharedObjectAndNTHandleAndAllocationFailedWhen2dCreatedThenReturnCorrectCode,
                            givenP010FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                            givenP016FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                            givenNV12FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                            givenP010FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                            givenP016FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                            givenPackedFormatWhenLookingForSurfaceFormatWithPackedNotSupportedThenReturnNull,
                            givenPackedFormatWhenLookingForSurfaceFormatWithPackedSupportedThenReturnValidFormat);

INSTANTIATE_TYPED_TEST_SUITE_P(D3DSharingTests, D3DTests, D3DTypes);

TEST(D3DSurfaceTest, givenD3DSurfaceWhenInvalidMemObjectIsPassedToValidateUpdateDataThenInvalidMemObjectErrorIsReturned) {
    class MockD3DSurface : public D3DSurface {
      public:
        MockD3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3DTypesHelper::D3D9::D3DTexture2d *surfaceStaging, cl_uint plane,
                       ImagePlane imagePlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable) : D3DSurface(context, surfaceInfo, surfaceStaging, plane,
                                                                                                                                          imagePlane, adapterType, sharedResource, lockable) {}
    };
    MockContext context;
    cl_dx9_surface_info_khr surfaceInfo = {};
    ImagePlane imagePlane = ImagePlane::noPlane;
    std::unique_ptr<D3DSurface> surface(new MockD3DSurface(&context, &surfaceInfo, nullptr, 0, imagePlane, 0, false, false));

    MockBuffer buffer;
    UpdateData updateData{context.getDevice(0)->getRootDeviceIndex()};
    updateData.memObject = &buffer;
    auto result = surface->validateUpdateData(updateData);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, result);
}

TEST(D3D9, givenD3D9BuilderAndExtensionEnableTrueWhenGettingExtensionsThenCorrectExtensionsListIsReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D9>>();
    builderFactory->extensionEnabled = true;
    EXPECT_TRUE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_intel_dx9_media_sharing")));
    EXPECT_TRUE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_dx9_media_sharing")));
}

TEST(D3D9, givenD3D9BuilderAndExtensionEnableFalseWhenGettingExtensionsThenDx9MediaSheringExtensionsAreNotReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D9>>();
    builderFactory->extensionEnabled = false;
    EXPECT_FALSE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_intel_dx9_media_sharing")));
    EXPECT_FALSE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_dx9_media_sharing")));
}

TEST(D3D10, givenD3D10BuilderWhenGettingExtensionsThenCorrectExtensionsListIsReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D10>>();
    EXPECT_TRUE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_d3d10_sharing")));
}

TEST(D3D10, givenD3D10BuilderAndExtensionEnableFalseWhenGettingExtensionsThenCorrectExtensionsListIsReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D10>>();
    builderFactory->extensionEnabled = false;
    EXPECT_FALSE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_d3d10_sharing")));
}

TEST(D3D11, givenD3D11BuilderWhenGettingExtensionsThenCorrectExtensionsListIsReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D11>>();
    EXPECT_TRUE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_d3d11_sharing")));
    EXPECT_TRUE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_intel_d3d11_nv12_media_sharing")));
}

TEST(D3D11, givenD3D11BuilderAndExtensionEnableFalseWhenGettingExtensionsThenCorrectExtensionsListIsReturned) {
    auto builderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D11>>();
    builderFactory->extensionEnabled = false;
    EXPECT_FALSE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_khr_d3d11_sharing")));
    EXPECT_FALSE(hasSubstr(builderFactory->getExtensions(nullptr), std::string("cl_intel_d3d11_nv12_media_sharing")));
}

TEST(D3DSharingFactory, givenEnabledFormatQueryAndFactoryWithD3DSharingsWhenGettingExtensionFunctionAddressThenFormatQueryFunctionsAreReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFormatQuery.set(true);
    SharingFactoryMock sharingFactory;

    auto function = sharingFactory.getExtensionFunctionAddress("clGetSupportedDX9MediaSurfaceFormatsINTEL");
    EXPECT_EQ(reinterpret_cast<void *>(clGetSupportedDX9MediaSurfaceFormatsINTEL), function);

    function = sharingFactory.getExtensionFunctionAddress("clGetSupportedD3D10TextureFormatsINTEL");
    EXPECT_EQ(reinterpret_cast<void *>(clGetSupportedD3D10TextureFormatsINTEL), function);

    function = sharingFactory.getExtensionFunctionAddress("clGetSupportedD3D11TextureFormatsINTEL");
    EXPECT_EQ(reinterpret_cast<void *>(clGetSupportedD3D11TextureFormatsINTEL), function);
}

TEST(D3D9SharingFactory, givenDriverInfoWhenVerifyExtensionSupportThenExtensionEnableIsSetCorrect) {
    class MockDriverInfo : public DriverInfoWindows {
      public:
        MockDriverInfo() : DriverInfoWindows("", PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue)) {}
        bool getMediaSharingSupport() override { return support; };
        bool support = true;
    };
    class MockSharingFactory : public SharingFactory {
      public:
        MockSharingFactory() {
            memcpy_s(savedState, sizeof(savedState), sharingContextBuilder, sizeof(sharingContextBuilder));
        }
        ~MockSharingFactory() {
            memcpy_s(sharingContextBuilder, sizeof(sharingContextBuilder), savedState, sizeof(savedState));
        }

        void prepare() {
            for (auto &builder : sharingContextBuilder) {
                builder = nullptr;
            }
            d3d9SharingBuilderFactory = std::make_unique<D3DSharingBuilderFactory<D3DTypesHelper::D3D9>>();
            sharingContextBuilder[SharingType::D3D9_SHARING] = d3d9SharingBuilderFactory.get();
        }

        using SharingFactory::sharingContextBuilder;
        std::unique_ptr<D3DSharingBuilderFactory<D3DTypesHelper::D3D9>> d3d9SharingBuilderFactory;
        decltype(SharingFactory::sharingContextBuilder) savedState;
    };

    auto driverInfo = std::make_unique<MockDriverInfo>();
    auto mockSharingFactory = std::make_unique<MockSharingFactory>();
    mockSharingFactory->prepare();

    driverInfo->support = true;
    mockSharingFactory->verifyExtensionSupport(driverInfo.get());
    EXPECT_TRUE(mockSharingFactory->d3d9SharingBuilderFactory->extensionEnabled);

    driverInfo->support = false;
    mockSharingFactory->verifyExtensionSupport(driverInfo.get());
    EXPECT_FALSE(mockSharingFactory->d3d9SharingBuilderFactory->extensionEnabled);
}

TEST(D3D9SharingFactory, givenDriverInfoWhenSetExtensionEnabledThenCorrectValueIsSet) {
    class MockDriverInfo : public DriverInfoWindows {
      public:
        MockDriverInfo() : DriverInfoWindows("", PhysicalDevicePciBusInfo(PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue, PhysicalDevicePciBusInfo::invalidValue)) {}
        bool getMediaSharingSupport() override { return support; };
        bool containsSetting(const char *setting) override {
            if (failingContainsSetting) {
                return false;
            }
            if (memcmp(setting, "UserModeDriverName", sizeof("UserModeDriverName")) == 0) {
                return true;
            } else if (memcmp(setting, "UserModeDriverNameWOW", sizeof("UserModeDriverNameWOW")) == 0) {
                return true;
            } else {
                return false;
            }
        }
        bool support = true;
        bool failingContainsSetting = false;
    };

    class SharingD3D9BuilderMock : public D3DSharingBuilderFactory<D3DTypesHelper::D3D9> {
      public:
    };

    auto driverInfo = std::make_unique<MockDriverInfo>();
    auto mockSharingFactory = std::make_unique<SharingD3D9BuilderMock>();

    driverInfo->support = true;
    mockSharingFactory->extensionEnabled = false;
    mockSharingFactory->setExtensionEnabled(driverInfo.get());
    EXPECT_TRUE(mockSharingFactory->extensionEnabled);

    driverInfo->failingContainsSetting = true;
    mockSharingFactory->extensionEnabled = false;
    mockSharingFactory->setExtensionEnabled(driverInfo.get());
    EXPECT_FALSE(mockSharingFactory->extensionEnabled);
}
} // namespace NEO
