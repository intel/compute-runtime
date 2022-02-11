/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_d3d_objects.h"

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
        bool failAlloc = false;
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
            if (failAlloc) {
                return nullptr;
            }
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(handle, properties, requireSpecificBitness, isHostIpcAllocation);
            alloc->setDefaultGmm(forceGmm);
            gmmOwnershipPassed = true;
            return alloc;
        }
        GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
            if (failAlloc) {
                return nullptr;
            }
            AllocationProperties properties(rootDeviceIndex, true, 0, AllocationType::INTERNAL_HOST_MEMORY, false, false, 0);
            auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(toOsHandle(handle), properties, false, false);
            alloc->setDefaultGmm(forceGmm);
            gmmOwnershipPassed = true;
            return alloc;
        }

        bool verifyValue = true;
        bool verifyHandle(osHandle handle, uint32_t rootDeviceIndex, bool) { return verifyValue; }

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
        ImageDescriptor imgDesc = {};
        imgDesc.imageHeight = 4;
        imgDesc.imageWidth = 4;
        imgDesc.imageDepth = 1;
        imgDesc.imageType = ImageType::Image2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        gmm = MockGmm::queryImgParams(pPlatform->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), imgInfo, false).release();
        mockGmmResInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

        mockMM->forceGmm = gmm;
    }

    void SetUp() override {
        VariableBackup<UltHwConfig> backup(&ultHwConfig);
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
        PlatformFixture::SetUp();
        rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
        context = new MockContext(pPlatform->getClDevice(0));
        context->preferD3dSharedResources = true;
        mockMM = std::make_unique<MockMM>(*context->getDevice(0)->getExecutionEnvironment());

        mockSharingFcns = new MockD3DSharingFunctions<T>();
        mockSharingFcns->checkFormatSupportSetParam1 = true;
        mockSharingFcns->checkFormatSupportParamsSet.pFormat = D3D11_FORMAT_SUPPORT_BUFFER | D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURE3D;

        context->setSharingFunctions(mockSharingFcns);
        context->memoryManager = mockMM.get();
        cmdQ = new MockCommandQueue(context, context->getDevice(0), 0, false);
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

    MockD3DSharingFunctions<T> *mockSharingFcns;
    MockContext *context;
    MockCommandQueue *cmdQ;
    char dummyD3DBuffer;
    char dummyD3DBufferStaging;
    char dummyD3DTexture;
    char dummyD3DTextureStaging;
    Gmm *gmm = nullptr;
    MockGmmResourceInfo *mockGmmResInfo = nullptr;

    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<MockMM> mockMM;

    uint8_t d3dMode = 0;
    uint32_t rootDeviceIndex = 0;
};
typedef ::testing::Types<D3DTypesHelper::D3D10, D3DTypesHelper::D3D11> D3DTypes;
} // namespace NEO
