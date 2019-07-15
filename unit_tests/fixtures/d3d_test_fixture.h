/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_d3d_objects.h"
#include "unit_tests/mocks/mock_gmm.h"

namespace NEO {
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
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness);
            alloc->setDefaultGmm(forceGmm);
            gmmOwnershipPassed = true;
            return alloc;
        }
        GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle) override {
            AllocationProperties properties(0, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY);
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle((osHandle)((UINT_PTR)handle), properties, false);
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
        mockMM = std::make_unique<MockMM>(*context->getDevice(0)->getExecutionEnvironment());

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
typedef ::testing::Types<D3DTypesHelper::D3D10, D3DTypesHelper::D3D11> D3DTypes;
} // namespace NEO