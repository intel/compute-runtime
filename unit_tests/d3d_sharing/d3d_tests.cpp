/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/d3d/cl_d3d_api.h"
#include "runtime/sharings/d3d/d3d_buffer.h"
#include "runtime/sharings/d3d/d3d_sharing.h"
#include "runtime/sharings/d3d/d3d_surface.h"
#include "runtime/sharings/d3d/d3d_texture.h"
#include "runtime/utilities/arrayref.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_d3d_objects.h"
#include "unit_tests/mocks/mock_gmm.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace OCLRT {
template <>
uint32_t MockD3DSharingFunctions<D3DTypesHelper::D3D10>::getDxgiDescCalled = 0;
template <>
uint32_t MockD3DSharingFunctions<D3DTypesHelper::D3D11>::getDxgiDescCalled = 0;
template <>
DXGI_ADAPTER_DESC MockD3DSharingFunctions<D3DTypesHelper::D3D10>::mockDxgiDesc = {{0}};
template <>
DXGI_ADAPTER_DESC MockD3DSharingFunctions<D3DTypesHelper::D3D11>::mockDxgiDesc = {{0}};
template <>
IDXGIAdapter *MockD3DSharingFunctions<D3DTypesHelper::D3D10>::getDxgiDescAdapterRequested = nullptr;
template <>
IDXGIAdapter *MockD3DSharingFunctions<D3DTypesHelper::D3D11>::getDxgiDescAdapterRequested = nullptr;

template <typename T>
class D3DTests : public PlatformFixture, public ::testing::Test {
  public:
    typedef typename T::D3DDevice D3DDevice;
    typedef typename T::D3DQuery D3DQuery;
    typedef typename T::D3DQueryDesc D3DQueryDesc;
    typedef typename T::D3DResource D3DResource;
    typedef typename T::D3DBufferDesc D3DBufferDesc;
    typedef typename T::D3DBufferObj D3DBufferObj;
    typedef typename T::D3DTexture2dDesc D3DTexture2dDesc;
    typedef typename T::D3DTexture3dDesc D3DTexture3dDesc;
    typedef typename T::D3DTexture2d D3DTexture2d;
    typedef typename T::D3DTexture3d D3DTexture3d;

    class MockMM : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, requireSpecificBitness);
            alloc->setDefaultGmm(forceGmm);
            gmmOwnershipPassed = true;
            return alloc;
        }
        GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)handle), false);
            alloc->setDefaultGmm(forceGmm);
            gmmOwnershipPassed = true;
            return alloc;
        }
        bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override {
            mapAuxGpuVACalled++;
            return mapAuxGpuVaRetValue;
        }
        Gmm *forceGmm = nullptr;
        bool gmmOwnershipPassed = false;
        uint32_t mapAuxGpuVACalled = 0u;
        bool mapAuxGpuVaRetValue = true;
    };

    void setupMockGmm() {
        cl_image_desc imgDesc = {};
        imgDesc.image_height = 4;
        imgDesc.image_width = 4;
        imgDesc.image_depth = 1;
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        gmm = MockGmm::queryImgParams(imgInfo).release();
        mockGmmResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());

        mockMM->forceGmm = gmm;
    }

    void SetUp() override {
        dbgRestore = new DebugManagerStateRestore();
        PlatformFixture::SetUp();
        context = new MockContext(pPlatform->getDevice(0));
        context->forcePreferD3dSharedResources(true);
        mockMM = std::make_unique<MockMM>(false, false, *context->getDevice(0)->getExecutionEnvironment());

        mockSharingFcns = new NiceMock<MockD3DSharingFunctions<T>>();
        context->setSharingFunctions(mockSharingFcns);
        context->setMemoryManager(mockMM.get());
        cmdQ = new MockCommandQueue(context, context->getDevice(0), 0);
        DebugManager.injectFcn = &mockSharingFcns->mockGetDxgiDesc;

        mockSharingFcns->mockTexture2dDesc.ArraySize = 1;
        mockSharingFcns->mockTexture2dDesc.MipLevels = 4;
        mockSharingFcns->mockTexture3dDesc.MipLevels = 4;

        setupMockGmm();
        if (context->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D10>>()) {
            ASSERT_EQ(0u, d3dMode);
            d3dMode = 10;
        }
        if (context->getSharing<D3DSharingFunctions<D3DTypesHelper::D3D11>>()) {
            ASSERT_EQ(0u, d3dMode);
            d3dMode = 11;
        }
        ASSERT_NE(0u, d3dMode);
    }

    void TearDown() override {
        delete cmdQ;
        delete context;
        if (!mockMM->gmmOwnershipPassed) {
            delete gmm;
        }
        PlatformFixture::TearDown();
        delete dbgRestore;
    }

    cl_int pickParam(cl_int d3d10, cl_int d3d11) {
        if (d3dMode == 10u) {
            return d3d10;
        }
        if (d3dMode == 11u) {
            return d3d11;
        }
        EXPECT_TRUE(false);
        return 0;
    }

    cl_mem createFromD3DBufferApi(cl_context context, cl_mem_flags flags, ID3D10Buffer *resource, cl_int *errcodeRet) {
        return clCreateFromD3D10BufferKHR(context, flags, resource, errcodeRet);
    }
    cl_mem createFromD3DBufferApi(cl_context context, cl_mem_flags flags, ID3D11Buffer *resource, cl_int *errcodeRet) {
        return clCreateFromD3D11BufferKHR(context, flags, resource, errcodeRet);
    }
    cl_mem createFromD3DTexture2DApi(cl_context context, cl_mem_flags flags, ID3D10Texture2D *resource,
                                     UINT subresource, cl_int *errcodeRet) {
        return clCreateFromD3D10Texture2DKHR(context, flags, resource, subresource, errcodeRet);
    }
    cl_mem createFromD3DTexture2DApi(cl_context context, cl_mem_flags flags, ID3D11Texture2D *resource,
                                     UINT subresource, cl_int *errcodeRet) {
        return clCreateFromD3D11Texture2DKHR(context, flags, resource, subresource, errcodeRet);
    }
    cl_mem createFromD3DTexture3DApi(cl_context context, cl_mem_flags flags, ID3D10Texture3D *resource,
                                     UINT subresource, cl_int *errcodeRet) {
        return clCreateFromD3D10Texture3DKHR(context, flags, resource, subresource, errcodeRet);
    }
    cl_mem createFromD3DTexture3DApi(cl_context context, cl_mem_flags flags, ID3D11Texture3D *resource,
                                     UINT subresource, cl_int *errcodeRet) {
        return clCreateFromD3D11Texture3DKHR(context, flags, resource, subresource, errcodeRet);
    }
    cl_int enqueueAcquireD3DObjectsApi(MockD3DSharingFunctions<D3DTypesHelper::D3D10> *mockFcns, cl_command_queue commandQueue, cl_uint numObjects,
                                       const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
        return clEnqueueAcquireD3D10ObjectsKHR(commandQueue, numObjects, memObjects, numEventsInWaitList, eventWaitList, event);
    }
    cl_int enqueueAcquireD3DObjectsApi(MockD3DSharingFunctions<D3DTypesHelper::D3D11> *mockFcns, cl_command_queue commandQueue, cl_uint numObjects,
                                       const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
        return clEnqueueAcquireD3D11ObjectsKHR(commandQueue, numObjects, memObjects, numEventsInWaitList, eventWaitList, event);
    }
    cl_int enqueueReleaseD3DObjectsApi(MockD3DSharingFunctions<D3DTypesHelper::D3D10> *mockFcns, cl_command_queue commandQueue, cl_uint numObjects,
                                       const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
        return clEnqueueReleaseD3D10ObjectsKHR(commandQueue, numObjects, memObjects, numEventsInWaitList, eventWaitList, event);
    }
    cl_int enqueueReleaseD3DObjectsApi(MockD3DSharingFunctions<D3DTypesHelper::D3D11> *mockFcns, cl_command_queue commandQueue, cl_uint numObjects,
                                       const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
        return clEnqueueReleaseD3D11ObjectsKHR(commandQueue, numObjects, memObjects, numEventsInWaitList, eventWaitList, event);
    }
    cl_int getDeviceIDsFromD3DApi(MockD3DSharingFunctions<D3DTypesHelper::D3D10> *mockFcns, cl_platform_id platform, cl_d3d10_device_source_khr d3dDeviceSource,
                                  void *d3dObject, cl_d3d10_device_set_khr d3dDeviceSet, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
        return clGetDeviceIDsFromD3D10KHR(platform, d3dDeviceSource, d3dObject, d3dDeviceSet, numEntries, devices, numDevices);
    }
    cl_int getDeviceIDsFromD3DApi(MockD3DSharingFunctions<D3DTypesHelper::D3D11> *mockFcns, cl_platform_id platform, cl_d3d10_device_source_khr d3dDeviceSource,
                                  void *d3dObject, cl_d3d10_device_set_khr d3dDeviceSet, cl_uint numEntries, cl_device_id *devices, cl_uint *numDevices) {
        return clGetDeviceIDsFromD3D11KHR(platform, d3dDeviceSource, d3dObject, d3dDeviceSet, numEntries, devices, numDevices);
    }

    NiceMock<MockD3DSharingFunctions<T>> *mockSharingFcns;
    MockContext *context;
    MockCommandQueue *cmdQ;
    char dummyD3DBuffer;
    char dummyD3DBufferStaging;
    char dummyD3DTexture;
    char dummyD3DTextureStaging;
    Gmm *gmm = nullptr;
    NiceMock<MockGmmResourceInfo> *mockGmmResInfo = nullptr;

    DebugManagerStateRestore *dbgRestore;
    std::unique_ptr<MockMM> mockMM;

    uint8_t d3dMode = 0;
};

TYPED_TEST_CASE_P(D3DTests);

TYPED_TEST_P(D3DTests, getDeviceIDsFromD3DWithSpecificDeviceSet) {
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

TYPED_TEST_P(D3DTests, getDeviceIDsFromD3DWithSpecificDeviceSource) {
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

TYPED_TEST_P(D3DTests, createFromD3DBufferKHRApi) {
    cl_int retVal;
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getSharedNTHandle(_, _))
        .Times(0);

    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, (D3DBufferObj *)&this->dummyD3DBuffer, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto buffer = castToObject<Buffer>(memObj);
    ASSERT_NE(nullptr, buffer);
    ASSERT_NE(nullptr, buffer->getSharingHandler().get());

    auto bufferObj = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());

    EXPECT_EQ((D3DResource *)&this->dummyD3DBuffer, *bufferObj->getResourceHandler());
    EXPECT_TRUE(buffer->getFlags() == CL_MEM_READ_WRITE);

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, givenNV12FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_NV12, OCLPlane::PLANE_Y, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, givenNV12FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_NV12;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_NV12, OCLPlane::PLANE_UV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, givenP010FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P010;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P010, OCLPlane::PLANE_Y, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, givenP010FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P010;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P010, OCLPlane::PLANE_UV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, givenP016FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P016;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P016, OCLPlane::PLANE_Y, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(2u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, givenP016FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams) {
    this->mockSharingFcns->mockTexture2dDesc.Format = DXGI_FORMAT_P016;
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 7, nullptr));
    ASSERT_NE(nullptr, image.get());

    auto expectedFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(DXGI_FORMAT_P016, OCLPlane::PLANE_UV, CL_MEM_READ_WRITE);
    EXPECT_TRUE(memcmp(expectedFormat, &image->getSurfaceFormatInfo(), sizeof(SurfaceFormatInfo)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(3u, mockGmmResInfo->arrayIndexPassedToGetOffset);
}

TYPED_TEST_P(D3DTests, createFromD3D2dTextureKHRApi) {
    cl_int retVal;
    EXPECT_CALL(*this->mockSharingFcns, createQuery(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getSharedNTHandle(_, _))
        .Times(0);

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, 1, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto image = castToObject<Image>(memObj);
    ASSERT_NE(nullptr, image);
    ASSERT_NE(nullptr, image->getSharingHandler().get());

    auto textureObj = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());

    EXPECT_EQ((D3DResource *)&this->dummyD3DTexture, *textureObj->getResourceHandler());
    EXPECT_TRUE(image->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D);
    EXPECT_EQ(1u, textureObj->getSubresource());

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, createFromD3D3dTextureKHRApi) {
    cl_int retVal;
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getSharedNTHandle(_, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, createQuery(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    auto memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture3d *)&this->dummyD3DTexture, 1, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto image = castToObject<Image>(memObj);
    ASSERT_NE(nullptr, image);
    ASSERT_NE(nullptr, image->getSharingHandler().get());

    auto textureObj = static_cast<D3DTexture<TypeParam> *>(image->getSharingHandler().get());

    EXPECT_EQ((D3DResource *)&this->dummyD3DTexture, *textureObj->getResourceHandler());
    EXPECT_TRUE(image->getFlags() == CL_MEM_READ_WRITE);
    EXPECT_TRUE(image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE3D);
    EXPECT_EQ(1u, textureObj->getSubresource());

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, givenSharedResourceFlagWhenCreateBufferThenStagingBufferEqualsPassedBuffer) {
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));
    EXPECT_CALL(*this->mockSharingFcns, createBuffer(_, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DBufferObj *)&this->dummyD3DBuffer, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DBufferObj *)&this->dummyD3DBuffer))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, createQuery(_))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DQuery *)1));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    auto d3dBuffer = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dBuffer);

    EXPECT_NE(nullptr, d3dBuffer->getQuery());
    EXPECT_TRUE(d3dBuffer->isSharedResource());
    EXPECT_EQ(&this->dummyD3DBuffer, d3dBuffer->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DBufferObj *)&this->dummyD3DBuffer))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dBuffer->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceFlagWhenCreateBufferThenCreateNewStagingBuffer) {
    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, createBuffer(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DBufferObj *)&this->dummyD3DBufferStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle((D3DBufferObj *)&this->dummyD3DBufferStaging, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, addRef((D3DBufferObj *)&this->dummyD3DBuffer))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, createQuery(_))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DQuery *)1));

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    auto d3dBuffer = static_cast<D3DBuffer<TypeParam> *>(buffer->getSharingHandler().get());
    ASSERT_NE(nullptr, d3dBuffer);

    EXPECT_NE(nullptr, d3dBuffer->getQuery());
    EXPECT_FALSE(d3dBuffer->isSharedResource());
    EXPECT_EQ(&this->dummyD3DBufferStaging, d3dBuffer->getResourceStaging());

    EXPECT_CALL(*this->mockSharingFcns, release((D3DBufferObj *)&this->dummyD3DBufferStaging))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DBufferObj *)&this->dummyD3DBuffer))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, release((D3DQuery *)d3dBuffer->getQuery()))
        .Times(1);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceBufferWhenAcquiredThenCopySubregion) {
    this->context->setInteropUserSyncEnabled(true);

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, createBuffer(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>((D3DBufferObj *)&this->dummyD3DBufferStaging));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getDeviceContext(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, copySubresourceRegion((D3DBufferObj *)&this->dummyD3DBufferStaging, 0u, (D3DBufferObj *)&this->dummyD3DBuffer, 0u))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, flushAndWait(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, releaseDeviceContext(_))
        .Times(1);

    EXPECT_CALL(*this->mockSharingFcns, getDeviceContext(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, copySubresourceRegion((D3DBufferObj *)&this->dummyD3DBuffer, 0u, (D3DBufferObj *)&this->dummyD3DBufferStaging, 0u))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, releaseDeviceContext(_))
        .Times(1);

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
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
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferWhenAcquiredThenDontCopySubregion) {
    this->context->setInteropUserSyncEnabled(true);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getDeviceContext(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, copySubresourceRegion(_, _, _, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, flushAndWait(_))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, releaseDeviceContext(_))
        .Times(1);

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncDisabledWhenAcquiredThenFlushOnAcquire) {
    this->context->setInteropUserSyncEnabled(false);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    ::testing::InSequence is;
    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));
    EXPECT_CALL(*this->mockSharingFcns, getSharedHandle(_, _))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, getDeviceContext(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, copySubresourceRegion(_, _, _, _))
        .Times(0);
    EXPECT_CALL(*this->mockSharingFcns, flushAndWait(_))
        .Times(1);
    EXPECT_CALL(*this->mockSharingFcns, releaseDeviceContext(_))
        .Times(1);

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());
    cl_mem bufferMem = (cl_mem)buffer.get();

    auto retVal = this->enqueueAcquireD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = this->enqueueReleaseD3DObjectsApi(this->mockSharingFcns, this->cmdQ, 1, &bufferMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TYPED_TEST_P(D3DTests, getPreferD3DSharedResources) {
    auto ctx = std::unique_ptr<MockContext>(new MockContext());
    cl_bool retBool = 0;
    size_t size = 0;
    auto param = this->pickParam(CL_CONTEXT_D3D10_PREFER_SHARED_RESOURCES_KHR, CL_CONTEXT_D3D11_PREFER_SHARED_RESOURCES_KHR);

    ctx->forcePreferD3dSharedResources(1u);
    auto retVal = ctx->getInfo(param, sizeof(retBool), &retBool, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_EQ(1u, retBool);

    ctx->forcePreferD3dSharedResources(0u);
    retVal = ctx->getInfo(param, sizeof(retBool), &retBool, &size);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), size);
    EXPECT_EQ(0u, retBool);
}

TYPED_TEST_P(D3DTests, getD3DResourceInfoFromMemObj) {
    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, (D3DBufferObj *)&this->dummyD3DBuffer, nullptr);
    ASSERT_NE(nullptr, memObj);
    auto param = this->pickParam(CL_MEM_D3D10_RESOURCE_KHR, CL_MEM_D3D11_RESOURCE_KHR);

    void *retBuffer = nullptr;
    size_t retSize = 0;
    clGetMemObjectInfo(memObj, param, sizeof(D3DBufferObj), &retBuffer, &retSize);
    EXPECT_EQ(sizeof(D3DBufferObj), retSize);
    EXPECT_EQ(&this->dummyD3DBuffer, retBuffer);

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, getD3DSubresourceInfoFromMemObj) {
    cl_int retVal;
    cl_uint subresource = 1u;
    auto param = this->pickParam(CL_IMAGE_D3D10_SUBRESOURCE_KHR, CL_IMAGE_D3D11_SUBRESOURCE_KHR);

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint retSubresource = 0;
    size_t retSize = 0;
    clGetImageInfo(memObj, param, sizeof(cl_uint), &retSubresource, &retSize);
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(subresource, retSubresource);

    clReleaseMemObject(memObj);
}

TYPED_TEST_P(D3DTests, givenTheSameD3DBufferWhenNextCreateIsCalledThenFail) {
    cl_int retVal;

    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, (D3DBufferObj *)&this->dummyD3DBuffer, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->at(0).second);
    auto memObj2 = this->createFromD3DBufferApi(this->context, CL_MEM_READ_WRITE, (D3DBufferObj *)&this->dummyD3DBuffer, &retVal);
    EXPECT_EQ(nullptr, memObj2);
    EXPECT_EQ(this->pickParam(CL_INVALID_D3D10_RESOURCE_KHR, CL_INVALID_D3D11_RESOURCE_KHR), retVal);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    clReleaseMemObject(memObj);
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
}

TYPED_TEST_P(D3DTests, givenD3DTextureWithTheSameSubresourceWhenNextCreateIsCalledThenFail) {
    cl_int retVal;
    cl_uint subresource = 1;

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    auto memObj2 = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    EXPECT_EQ(nullptr, memObj2);
    EXPECT_EQ(this->pickParam(CL_INVALID_D3D10_RESOURCE_KHR, CL_INVALID_D3D11_RESOURCE_KHR), retVal);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    subresource++;
    this->setupMockGmm(); // setup new mock for new resource
    auto memObj3 = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, this->mockSharingFcns->getTrackedResourcesVector()->size());

    clReleaseMemObject(memObj);
    EXPECT_EQ(1u, this->mockSharingFcns->getTrackedResourcesVector()->size());
    clReleaseMemObject(memObj3);
    EXPECT_EQ(0u, this->mockSharingFcns->getTrackedResourcesVector()->size());
}

TYPED_TEST_P(D3DTests, givenInvalidSubresourceWhenCreateTexture2dIsCalledThenFail) {
    cl_int retVal;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;
    cl_uint subresource = 16;

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    subresource = 20;
    memObj = this->createFromD3DTexture2DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture2d *)&this->dummyD3DTexture, subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TYPED_TEST_P(D3DTests, givenInvalidSubresourceWhenCreateTexture3dIsCalledThenFail) {
    cl_int retVal;
    this->mockSharingFcns->mockTexture3dDesc.MipLevels = 4;
    cl_uint subresource = 16;

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    auto memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture3d *)&this->dummyD3DTexture, subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    subresource = 20;
    memObj = this->createFromD3DTexture3DApi(this->context, CL_MEM_READ_WRITE, (D3DTexture3d *)&this->dummyD3DTexture, subresource, &retVal);
    EXPECT_EQ(nullptr, memObj);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TYPED_TEST_P(D3DTests, givenReadonlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::readOnly().size(), 0u);
    for (auto &format : SurfaceFormats::readOnly()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.OCLImageFormat.image_channel_order == CL_RGBA ||
            format.OCLImageFormat.image_channel_order == CL_BGRA ||
            format.OCLImageFormat.image_channel_order == CL_RG ||
            format.OCLImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.GMMSurfaceFormat, CL_MEM_READ_ONLY);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

TYPED_TEST_P(D3DTests, givenWriteOnlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::writeOnly().size(), 0u);
    for (auto &format : SurfaceFormats::writeOnly()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.OCLImageFormat.image_channel_order == CL_RGBA ||
            format.OCLImageFormat.image_channel_order == CL_BGRA ||
            format.OCLImageFormat.image_channel_order == CL_RG ||
            format.OCLImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.GMMSurfaceFormat, CL_MEM_WRITE_ONLY);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

TYPED_TEST_P(D3DTests, givenReadWriteFormatWhenLookingForSurfaceFormatThenReturnValidFormat) {
    EXPECT_GT(SurfaceFormats::readWrite().size(), 0u);
    for (auto &format : SurfaceFormats::readWrite()) {
        // only RGBA, BGRA, RG, R allowed for D3D
        if (format.OCLImageFormat.image_channel_order == CL_RGBA ||
            format.OCLImageFormat.image_channel_order == CL_BGRA ||
            format.OCLImageFormat.image_channel_order == CL_RG ||
            format.OCLImageFormat.image_channel_order == CL_R) {
            auto surfaceFormat = D3DSharing<TypeParam>::findSurfaceFormatInfo(format.GMMSurfaceFormat, CL_MEM_READ_WRITE);
            ASSERT_NE(nullptr, surfaceFormat);
            EXPECT_EQ(&format, surfaceFormat);
        }
    }
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDontDoExplicitFinish) {
    this->context->setInteropUserSyncEnabled(true);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, Device *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish(bool dcFlush) override {
            finishCalled++;
            dcFlushRequested = dcFlush;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
        bool dcFlushRequested = false;
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
        MockCmdQ(Context *context, Device *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish(bool dcFlush) override {
            finishCalled++;
            dcFlushRequested = dcFlush;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
        bool dcFlushRequested = false;
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
    EXPECT_TRUE(mockCmdQ->dcFlushRequested);
}

TYPED_TEST_P(D3DTests, givenSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishOnce) {
    this->context->setInteropUserSyncEnabled(false);
    this->mockSharingFcns->mockBufferDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_CALL(*this->mockSharingFcns, getBufferDesc(_, _))
        .Times(1)
        .WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockBufferDesc));

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, Device *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish(bool dcFlush) override {
            finishCalled++;
            dcFlushRequested = dcFlush;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
        bool dcFlushRequested = false;
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
    EXPECT_TRUE(mockCmdQ->dcFlushRequested);
}

TYPED_TEST_P(D3DTests, givenNonSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDoExplicitFinishOnce) {
    this->context->setInteropUserSyncEnabled(true);

    class MockCmdQ : public CommandQueue {
      public:
        MockCmdQ(Context *context, Device *device, const cl_queue_properties *properties) : CommandQueue(context, device, properties){};
        cl_int finish(bool dcFlush) override {
            finishCalled++;
            dcFlushRequested = dcFlush;
            return CL_SUCCESS;
        }
        uint32_t finishCalled = 0;
        bool dcFlushRequested = false;
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
    EXPECT_TRUE(mockCmdQ->dcFlushRequested);
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
    auto ctx = std::unique_ptr<MockContext>(Context::create<MockContext>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, ctx.get());
    EXPECT_EQ(1u, ctx->peekPreferD3dSharedResources());
    EXPECT_NE(nullptr, ctx->getSharing<D3DSharingFunctions<TypeParam>>());
}

TYPED_TEST_P(D3DTests, givenSharedNTHandleFlagWhenCreate2dTextureThenGetNtHandle) {
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

TYPED_TEST_P(D3DTests, givenSharedNTHandleFlagWhenCreate3dTextureThenGetNtHandle) {
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

TYPED_TEST_P(D3DTests, fillBufferDesc) {
    D3DBufferDesc requestedDesc = {};
    D3DBufferDesc expectedDesc = {};
    expectedDesc.ByteWidth = 10;
    expectedDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    this->mockSharingFcns->fillCreateBufferDesc(requestedDesc, 10);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3DBufferDesc)) == 0);
}

TYPED_TEST_P(D3DTests, fillTexture2dDesc) {
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

TYPED_TEST_P(D3DTests, fillTexture3dDesc) {
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
    const SurfaceFormatInfo *surfaceFormat;
    DXGI_FORMAT testFormat[] = {DXGI_FORMAT::DXGI_FORMAT_NV12, DXGI_FORMAT::DXGI_FORMAT_P010, DXGI_FORMAT::DXGI_FORMAT_P016};
    int channelDataType[] = {CL_UNORM_INT8, CL_UNORM_INT16, CL_UNORM_INT16};
    for (int n = 0; n < 3; n++) {
        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], OCLPlane::NO_PLANE, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], OCLPlane::PLANE_U, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], OCLPlane::PLANE_UV, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], OCLPlane::PLANE_V, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_RG);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);

        surfaceFormat = D3DTexture<TypeParam>::findYuvSurfaceFormatInfo(testFormat[n], OCLPlane::PLANE_Y, CL_MEM_READ_WRITE);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_order == CL_R);
        EXPECT_TRUE(surfaceFormat->OCLImageFormat.image_channel_data_type == channelDataType[n]);
    }
}

TYPED_TEST_P(D3DTests, inForced32BitAddressingBufferCreatedHas32BitAllocation) {

    auto buffer = std::unique_ptr<Buffer>(D3DBuffer<TypeParam>::create(this->context, (D3DBufferObj *)&this->dummyD3DBuffer, CL_MEM_READ_WRITE, nullptr));
    ASSERT_NE(nullptr, buffer.get());

    auto *allocation = buffer->getGraphicsAllocation();
    EXPECT_NE(nullptr, allocation);

    EXPECT_TRUE(allocation->is32BitAllocation());
}

REGISTER_TYPED_TEST_CASE_P(D3DTests,
                           getDeviceIDsFromD3DWithSpecificDeviceSet,
                           getDeviceIDsFromD3DWithSpecificDeviceSource,
                           givenNonIntelVendorWhenGetDeviceIdIsCalledThenReturnError,
                           createFromD3DBufferKHRApi,
                           createFromD3D2dTextureKHRApi,
                           createFromD3D3dTextureKHRApi,
                           givenSharedResourceFlagWhenCreateBufferThenStagingBufferEqualsPassedBuffer,
                           givenNonSharedResourceFlagWhenCreateBufferThenCreateNewStagingBuffer,
                           givenNonSharedResourceBufferWhenAcquiredThenCopySubregion,
                           givenSharedResourceBufferWhenAcquiredThenDontCopySubregion,
                           givenSharedResourceBufferAndInteropUserSyncDisabledWhenAcquiredThenFlushOnAcquire,
                           getPreferD3DSharedResources,
                           getD3DResourceInfoFromMemObj,
                           getD3DSubresourceInfoFromMemObj,
                           givenTheSameD3DBufferWhenNextCreateIsCalledThenFail,
                           givenD3DTextureWithTheSameSubresourceWhenNextCreateIsCalledThenFail,
                           givenInvalidSubresourceWhenCreateTexture2dIsCalledThenFail,
                           givenInvalidSubresourceWhenCreateTexture3dIsCalledThenFail,
                           givenReadonlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                           givenWriteOnlyFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                           givenReadWriteFormatWhenLookingForSurfaceFormatThenReturnValidFormat,
                           givenSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDontDoExplicitFinish,
                           givenNonSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishTwice,
                           givenSharedResourceBufferAndInteropUserSyncDisabledWhenReleaseIsCalledThenDoExplicitFinishOnce,
                           givenNonSharedResourceBufferAndInteropUserSyncEnabledWhenReleaseIsCalledThenDoExplicitFinishOnce,
                           givenSharedResourceFlagWhenCreate2dTextureThenStagingTextureEqualsPassedTexture,
                           givenNonSharedResourceFlagWhenCreate2dTextureThenCreateStagingTexture,
                           givenSharedResourceFlagWhenCreate3dTextureThenStagingTextureEqualsPassedTexture,
                           givenNonSharedResourceFlagWhenCreate3dTextureThenCreateStagingTexture,
                           givenD3DDeviceParamWhenContextCreationThenSetProperValues,
                           givenSharedNTHandleFlagWhenCreate2dTextureThenGetNtHandle,
                           givenSharedNTHandleFlagWhenCreate3dTextureThenGetNtHandle,
                           fillBufferDesc,
                           fillTexture2dDesc,
                           fillTexture3dDesc,
                           givenPlaneWhenFindYuvSurfaceCalledThenReturnValidImgFormat,
                           givenNV12FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                           givenP010FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                           givenP016FormatAndEvenPlaneWhen2dCreatedThenSetPlaneParams,
                           givenNV12FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                           givenP010FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                           givenP016FormatAndOddPlaneWhen2dCreatedThenSetPlaneParams,
                           inForced32BitAddressingBufferCreatedHas32BitAllocation);

typedef ::testing::Types<D3DTypesHelper::D3D10, D3DTypesHelper::D3D11> D3DTypes;
INSTANTIATE_TYPED_TEST_CASE_P(D3DSharingTests, D3DTests, D3DTypes);

template <typename T>
class D3DAuxTests : public D3DTests<T> {};
TYPED_TEST_CASE_P(D3DAuxTests);

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    mockMM->mapAuxGpuVaRetValue = false;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    mockMM->mapAuxGpuVaRetValue = false;
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

REGISTER_TYPED_TEST_CASE_P(D3DAuxTests,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed,
                           given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed,
                           given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed);

INSTANTIATE_TYPED_TEST_CASE_P(D3DSharingTests, D3DAuxTests, D3DTypes);

TEST(D3DSurfaceTest, givenD3DSurfaceWhenInvalidMemObjectIsPassedToValidateUpdateDataThenInvalidMemObjectErrorIsReturned) {
    class MockD3DSurface : public D3DSurface {
      public:
        MockD3DSurface(Context *context, cl_dx9_surface_info_khr *surfaceInfo, D3DTypesHelper::D3D9::D3DTexture2d *surfaceStaging, cl_uint plane,
                       OCLPlane oclPlane, cl_dx9_media_adapter_type_khr adapterType, bool sharedResource, bool lockable) : D3DSurface(context, surfaceInfo, surfaceStaging, plane,
                                                                                                                                      oclPlane, adapterType, sharedResource, lockable) {}
    };
    MockContext context;
    cl_dx9_surface_info_khr surfaceInfo = {};
    OCLPlane oclPlane = OCLPlane::NO_PLANE;
    std::unique_ptr<D3DSurface> surface(new MockD3DSurface(&context, &surfaceInfo, nullptr, 0, oclPlane, 0, false, false));

    MockBuffer buffer;
    UpdateData updateData;
    updateData.memObject = &buffer;
    auto result = surface->validateUpdateData(updateData);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, result);
}

static const DXGI_FORMAT DXGIformats[] = {
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB,
    DXGI_FORMAT_AYUV,
    DXGI_FORMAT_Y410,
    DXGI_FORMAT_Y416,
    DXGI_FORMAT_NV12,
    DXGI_FORMAT_P010,
    DXGI_FORMAT_P016,
    DXGI_FORMAT_420_OPAQUE,
    DXGI_FORMAT_YUY2,
    DXGI_FORMAT_Y210,
    DXGI_FORMAT_Y216,
    DXGI_FORMAT_NV11,
    DXGI_FORMAT_AI44,
    DXGI_FORMAT_IA44,
    DXGI_FORMAT_P8,
    DXGI_FORMAT_A8P8,
    DXGI_FORMAT_B4G4R4A4_UNORM,
    DXGI_FORMAT_P208,
    DXGI_FORMAT_V208,
    DXGI_FORMAT_V408,
    DXGI_FORMAT_FORCE_UINT};

template <typename T>
struct clIntelSharingFormatQueryDX1X : public PlatformFixture, public ::testing::Test {
    std::vector<DXGI_FORMAT> retrievedFormats;
    ArrayRef<const DXGI_FORMAT> availableFormats;

    NiceMock<MockD3DSharingFunctions<T>> *mockSharingFcns;
    MockContext *context;
    cl_uint numImageFormats;
    cl_int retVal;
    size_t retSize;

    void SetUp() override {
        PlatformFixture::SetUp();
        context = new MockContext(pPlatform->getDevice(0));
        mockSharingFcns = new NiceMock<MockD3DSharingFunctions<T>>();
        context->setSharingFunctions(mockSharingFcns);

        availableFormats = ArrayRef<const DXGI_FORMAT>(DXGIformats);
        retrievedFormats.assign(availableFormats.size(), DXGI_FORMAT_UNKNOWN);
    }
    void TearDown() override {
        delete context;
        PlatformFixture::TearDown();
    }
};

typedef clIntelSharingFormatQueryDX1X<D3DTypesHelper::D3D10> clIntelSharingFormatQueryDX10;
typedef clIntelSharingFormatQueryDX1X<D3DTypesHelper::D3D11> clIntelSharingFormatQueryDX11;

TEST_F(clIntelSharingFormatQueryDX10, givenInvalidContextWhenDX10TextureFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedDX10TextureFormatsINTEL(NULL, CL_MEM_READ_WRITE, 0,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX10, givenValidParametersWhenRequestedDX10TextureFormatsThenTheResultIsASubsetOfKnownFormats) {
    retVal = clGetSupportedDX10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    for (cl_uint i = 0; i < numImageFormats; ++i) {
        EXPECT_NE(std::find(availableFormats.begin(),
                            availableFormats.end(),
                            retrievedFormats[i]),
                  availableFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX10, givenValidParametersWhenRequestedDX10TextureFormatsTwiceThenTheResultsAreTheSame) {
    retVal = clGetSupportedDX10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    std::vector<DXGI_FORMAT> formatsRetrievedForTheSecondTime(availableFormats.size());
    cl_uint anotherNumImageFormats;
    retVal = clGetSupportedDX10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(formatsRetrievedForTheSecondTime.size()),
                                                   &formatsRetrievedForTheSecondTime[0], &anotherNumImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_EQ(numImageFormats, anotherNumImageFormats);
    ASSERT_EQ(memcmp(&retrievedFormats[0], &formatsRetrievedForTheSecondTime[0], numImageFormats * sizeof(DXGI_FORMAT)), 0);
}

TEST_F(clIntelSharingFormatQueryDX10, givenNullFormatsWhenRequestedDX10TextureFormatsThenNumImageFormatsIsSane) {
    retVal = clGetSupportedDX10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_LE(0U, numImageFormats);
    ASSERT_LE(numImageFormats, static_cast<cl_uint>(availableFormats.size()));
}

TEST_F(clIntelSharingFormatQueryDX10, givenNullPointersWhenRequestedDX10TextureFormatsThenCLSuccessIsReturned) {
    retVal = clGetSupportedDX10TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, static_cast<cl_uint>(retrievedFormats.size()), nullptr, nullptr);
    ASSERT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clIntelSharingFormatQueryDX11, givenInvalidContextWhenDX11TextureFormatsRequestedThenInvalidContextError) {
    retVal = clGetSupportedDX11TextureFormatsINTEL(nullptr, CL_MEM_READ_WRITE, 0,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clIntelSharingFormatQueryDX11, givenValidParametersWhenRequestedDX11TextureFormatsThenTheResultIsASubsetOfKnownFormats) {
    retVal = clGetSupportedDX11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    for (cl_uint i = 0; i < numImageFormats; ++i) {
        EXPECT_NE(std::find(availableFormats.begin(),
                            availableFormats.end(),
                            retrievedFormats[i]),
                  availableFormats.end());
    }
}

TEST_F(clIntelSharingFormatQueryDX11, givenNullFormatsWhenRequestedDX11TextureFormatsThenNumImageFormatsIsSane) {
    retVal = clGetSupportedDX11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_LE(0U, numImageFormats);
    ASSERT_LE(numImageFormats, static_cast<cl_uint>(availableFormats.size()));
}

TEST_F(clIntelSharingFormatQueryDX11, givenNullPointersWhenRequestedDX11TextureFormatsThenCLSuccessIsReturned) {
    retVal = clGetSupportedDX11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, static_cast<cl_uint>(retrievedFormats.size()), nullptr, nullptr);
    ASSERT_EQ(retVal, CL_SUCCESS);
}

TEST_F(clIntelSharingFormatQueryDX11, givenValidParametersWhenRequestedDX11TextureFormatsTwiceThenTheResultsAreTheSame) {
    retVal = clGetSupportedDX11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(retrievedFormats.size()),
                                                   &retrievedFormats[0], &numImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    std::vector<DXGI_FORMAT> formatsRetrievedForTheSecondTime(availableFormats.size());
    cl_uint anotherNumImageFormats;
    retVal = clGetSupportedDX11TextureFormatsINTEL(context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                   static_cast<cl_uint>(formatsRetrievedForTheSecondTime.size()),
                                                   &formatsRetrievedForTheSecondTime[0], &anotherNumImageFormats);
    ASSERT_EQ(retVal, CL_SUCCESS);
    ASSERT_EQ(numImageFormats, anotherNumImageFormats);
    ASSERT_EQ(memcmp(&retrievedFormats[0], &formatsRetrievedForTheSecondTime[0], numImageFormats * sizeof(DXGI_FORMAT)), 0);
}

} // namespace OCLRT
