/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/api/api.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/os_interface/windows/d3d_sharing_functions.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_d3d_objects.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {
template <>
uint32_t MockD3DSharingFunctions<D3DTypesHelper::D3D9>::getDxgiDescCalled = 0;
template <>
DXGI_ADAPTER_DESC MockD3DSharingFunctions<D3DTypesHelper::D3D9>::mockDxgiDesc = {{0}};
template <>
IDXGIAdapter *MockD3DSharingFunctions<D3DTypesHelper::D3D9>::getDxgiDescAdapterRequested = nullptr;

class MockMM : public OsAgnosticMemoryManager {
  public:
    MockMM(const ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(const_cast<ExecutionEnvironment &>(executionEnvironment)){};
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, requireSpecificBitness, isHostIpcAllocation, reuseSharedAllocation, mapPointer);
        alloc->setDefaultGmm(forceGmm);
        gmmOwnershipPassed = true;
        return alloc;
    }
    GraphicsAllocation *allocateGraphicsMemoryForImage(const AllocationData &allocationData) override {
        auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), *allocationData.imgInfo, StorageInfo{}, false);
        AllocationProperties properties(allocationData.rootDeviceIndex, false, nullptr, AllocationType::sharedImage, DeviceBitfield{});
        OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
        auto alloc = OsAgnosticMemoryManager::createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        alloc->setDefaultGmm(forceGmm);
        gmmOwnershipPassed = true;
        return alloc;
    }

    void *lockResourceImpl(GraphicsAllocation &allocation) override {
        lockResourceCalled++;
        EXPECT_EQ(expectedLockingAllocation, &allocation);
        return lockResourceReturnValue;
    }
    void unlockResourceImpl(GraphicsAllocation &allocation) override {
        unlockResourceCalled++;
        EXPECT_EQ(expectedLockingAllocation, &allocation);
    }

    int32_t lockResourceCalled = 0;
    int32_t unlockResourceCalled = 0;
    GraphicsAllocation *expectedLockingAllocation = nullptr;
    void *lockResourceReturnValue = nullptr;
    Gmm *forceGmm = nullptr;
    bool gmmOwnershipPassed = false;
};

class D3D9Tests : public PlatformFixture, public ::testing::Test {
  public:
    typedef typename D3DTypesHelper::D3D9 D3D9;
    typedef typename D3D9::D3DDevice D3DDevice;
    typedef typename D3D9::D3DQuery D3DQuery;
    typedef typename D3D9::D3DQueryDesc D3DQueryDesc;
    typedef typename D3D9::D3DResource D3DResource;
    typedef typename D3D9::D3DTexture2dDesc D3DTexture2dDesc;
    typedef typename D3D9::D3DTexture2d D3DTexture2d;

    void setupMockGmm() {
        ImageDescriptor imgDesc = {};
        imgDesc.imageHeight = 10;
        imgDesc.imageWidth = 10;
        imgDesc.imageDepth = 1;
        imgDesc.imageType = ImageType::image2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        gmm = MockGmm::queryImgParams(pPlatform->getClDevice(0)->getGmmHelper(), imgInfo, false).release();
        mockGmmResInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

        memoryManager->forceGmm = gmm;
    }

    void SetUp() override {
        PlatformFixture::setUp();
        memoryManager = std::make_unique<MockMM>(*pPlatform->peekExecutionEnvironment());
        context = new MockContext(pPlatform->getClDevice(0));
        context->preferD3dSharedResources = true;
        context->memoryManager = memoryManager.get();

        mockSharingFcns = new MockD3DSharingFunctions<D3D9>();
        context->setSharingFunctions(mockSharingFcns);
        cmdQ = new MockCommandQueue(context, context->getDevice(0), 0, false);
        debugManager.injectFcn = reinterpret_cast<void *>(&mockSharingFcns->mockGetDxgiDesc);

        surfaceInfo.resource = reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface);

        mockSharingFcns->mockTexture2dDesc.Format = D3DFMT_R32F;
        mockSharingFcns->mockTexture2dDesc.Height = 10;
        mockSharingFcns->mockTexture2dDesc.Width = 10;

        setupMockGmm();
    }

    void TearDown() override {
        delete cmdQ;
        delete context;
        if (!memoryManager->gmmOwnershipPassed) {
            delete gmm;
        }
        PlatformFixture::tearDown();
    }

    MockD3DSharingFunctions<D3D9> *mockSharingFcns;
    MockContext *context;
    MockCommandQueue *cmdQ;
    DebugManagerStateRestore dbgRestore;
    uint64_t dummyD3DSurface{};
    uint64_t dummyD3DSurfaceStaging{};
    cl_dx9_surface_info_khr surfaceInfo = {};

    Gmm *gmm = nullptr;
    MockGmmResourceInfo *mockGmmResInfo = nullptr;
    std::unique_ptr<MockMM> memoryManager;
};

TEST_F(D3D9Tests, givenD3DDeviceParamWhenContextCreationThenSetProperValues) {
    cl_device_id deviceID = context->getDevice(0);
    cl_platform_id pid[1] = {pPlatform};
    char expectedDevice;

    cl_context_properties validAdapters[6] = {CL_CONTEXT_ADAPTER_D3D9_KHR, CL_CONTEXT_ADAPTER_D3D9EX_KHR, CL_CONTEXT_ADAPTER_DXVA_KHR,
                                              CL_CONTEXT_D3D9_DEVICE_INTEL, CL_CONTEXT_D3D9EX_DEVICE_INTEL, CL_CONTEXT_DXVA_DEVICE_INTEL};

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_ADAPTER_D3D9_KHR, (cl_context_properties)&expectedDevice, 0};

    std::unique_ptr<MockContext> ctx(nullptr);
    cl_int retVal = CL_SUCCESS;

    for (int i = 0; i < 6; i++) {
        validProperties[2] = validAdapters[i];
        ctx.reset(Context::create<MockContext>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, ctx.get());
        EXPECT_NE(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>());
        EXPECT_TRUE(reinterpret_cast<D3DDevice *>(&expectedDevice) == ctx->getSharing<D3DSharingFunctions<D3D9>>()->getDevice());
    }
}

TEST_F(D3D9Tests, WhenGetDeviceIdThenOneCorrectDeviceIsReturned) {
    cl_device_id expectedDevice = *devices;
    cl_device_id device = 0;
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDsFromDX9MediaAdapterKHR(platform(), 1, nullptr, nullptr, 1, 1, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);

    retVal = clGetDeviceIDsFromDX9INTEL(platform(), 1, nullptr, 1, 1, &device, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDevice, device);
    EXPECT_EQ(1u, numDevices);
}

TEST_F(D3D9Tests, WhenCreatingSurfaceThenImagePropertiesAreSetCorrectly) {
    cl_int retVal;
    cl_image_format expectedImgFormat = {};
    ImagePlane imagePlane = ImagePlane::noPlane;
    D3DSurface::findImgFormat(mockSharingFcns->mockTexture2dDesc.Format, expectedImgFormat, 0, imagePlane);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto memObj = clCreateFromDX9MediaSurfaceKHR(context, CL_MEM_READ_WRITE, 0, &surfaceInfo, 0, &retVal);
    ASSERT_NE(nullptr, memObj);
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto image = castToObject<Image>(memObj);
    EXPECT_NE(nullptr, image->getSharingHandler());

    EXPECT_TRUE(CL_MEM_READ_WRITE == image->getFlags());

    EXPECT_TRUE(expectedImgFormat.image_channel_data_type == image->getImageFormat().image_channel_data_type);
    EXPECT_TRUE(expectedImgFormat.image_channel_order == image->getImageFormat().image_channel_order);

    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == image->getImageDesc().image_type);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, image->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, image->getImageDesc().image_height);

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST(D3D9SimpleTests, givenWrongFormatWhenFindIsCalledThenErrorIsReturned) {
    cl_image_format expectedImgFormat = {};
    ImagePlane imagePlane = ImagePlane::noPlane;
    auto status = D3DSurface::findImgFormat(D3DFMT_FORCE_DWORD, expectedImgFormat, 0, imagePlane);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, status);
}

TEST_F(D3D9Tests, WhenCreatingSurfaceIntelThenImagePropertiesAreSetCorrectly) {
    cl_int retVal;
    cl_image_format expectedImgFormat = {};
    ImagePlane imagePlane = ImagePlane::noPlane;
    D3DSurface::findImgFormat(mockSharingFcns->mockTexture2dDesc.Format, expectedImgFormat, 0, imagePlane);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto memObj = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_READ_WRITE, surfaceInfo.resource, surfaceInfo.shared_handle, 0, &retVal);
    ASSERT_NE(nullptr, memObj);

    auto image = castToObject<Image>(memObj);
    EXPECT_NE(nullptr, image->getSharingHandler());

    EXPECT_TRUE(CL_MEM_READ_WRITE == image->getFlags());

    EXPECT_TRUE(expectedImgFormat.image_channel_data_type == image->getImageFormat().image_channel_data_type);
    EXPECT_TRUE(expectedImgFormat.image_channel_order == image->getImageFormat().image_channel_order);

    EXPECT_TRUE(CL_MEM_OBJECT_IMAGE2D == image->getImageDesc().image_type);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, image->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, image->getImageDesc().image_height);

    clReleaseMemObject(memObj);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, givenD3DHandleWhenCreatingSharedSurfaceThenAllocationTypeImageIsSet) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = reinterpret_cast<HANDLE>(1);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(AllocationType::sharedImage, graphicsAllocation->getAllocationType());

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenUPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)1;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch()) / 2;
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenVPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)1;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch()) / 2;
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenUVPlaneWhenCreateSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)1;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenYPlaneWhenCreateSurfaceThenDontChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)1;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenUPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenVPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenUVPlaneWhenCreateNonSharedSurfaceThenChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width / 2, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height / 2, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenYPlaneWhenCreateNonSharedSurfaceThenDontChangeWidthHeightAndPitch) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Width, sharedImg->getImageDesc().image_width);
    EXPECT_EQ(mockSharingFcns->mockTexture2dDesc.Height, sharedImg->getImageDesc().image_height);
    size_t expectedRowPitch = static_cast<size_t>(mockGmmResInfo->getRenderPitch());
    EXPECT_EQ(expectedRowPitch, sharedImg->getImageDesc().image_row_pitch);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenNV12FormatAndInvalidPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 2, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenYV12FormatAndInvalidPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 3, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenNonPlaneFormatAndNonZeroPlaneWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = D3DFORMAT::D3DFMT_A16B16G16R16;
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(D3D9Tests, givenNullResourceWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    auto img = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_READ_WRITE, nullptr, 0, 0, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenNonDefaultPoolWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Pool = D3DPOOL_SYSTEMMEM;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 1, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenAlreadyUsedSurfaceWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    surfaceInfo.resource = reinterpret_cast<IDirect3DSurface9 *>(0x8);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    std::unique_ptr<Image> img(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal));
    EXPECT_NE(nullptr, img.get());

    img.reset(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal));
    EXPECT_EQ(nullptr, img.get());
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);
}

TEST_F(D3D9Tests, givenNotSupportedFormatWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto img = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(nullptr, img);
    EXPECT_EQ(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, retVal);
}

TEST_F(D3D9Tests, GivenMediaSurfaceInfoKhrWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint expectedPlane = 2;
    cl_dx9_surface_info_khr getSurfaceInfo = {};
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getMemObjectInfo(CL_MEM_DX9_MEDIA_SURFACE_INFO_KHR, sizeof(cl_dx9_surface_info_khr), &getSurfaceInfo, &retSize);
    EXPECT_EQ(getSurfaceInfo.resource, surfaceInfo.resource);
    EXPECT_EQ(getSurfaceInfo.shared_handle, surfaceInfo.shared_handle);
    EXPECT_EQ(sizeof(cl_dx9_surface_info_khr), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, GivenResourceIntelWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint expectedPlane = 2;
    cl_dx9_surface_info_khr getSurfaceInfo = {};
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    getSurfaceInfo = {};
    sharedImg->getMemObjectInfo(CL_MEM_DX9_RESOURCE_INTEL, sizeof(IDirect3DSurface9 *), &getSurfaceInfo.resource, &retSize);
    EXPECT_EQ(getSurfaceInfo.resource, surfaceInfo.resource);
    EXPECT_EQ(sizeof(IDirect3DSurface9 *), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, GivenSharedHandleIntelWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint expectedPlane = 2;
    cl_dx9_surface_info_khr getSurfaceInfo = {};
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getMemObjectInfo(CL_MEM_DX9_SHARED_HANDLE_INTEL, sizeof(IDirect3DSurface9 *), &getSurfaceInfo.shared_handle, &retSize);
    EXPECT_EQ(getSurfaceInfo.shared_handle, surfaceInfo.shared_handle);
    EXPECT_EQ(sizeof(IDirect3DSurface9 *), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, GivenMediaAdapterTypeKhrWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr adapterType = 0;
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint expectedPlane = 2;
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getMemObjectInfo(CL_MEM_DX9_MEDIA_ADAPTER_TYPE_KHR, sizeof(cl_dx9_media_adapter_type_khr), &adapterType, &retSize);
    EXPECT_EQ(expectedAdapterType, adapterType);
    EXPECT_EQ(sizeof(cl_dx9_media_adapter_type_khr), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, GivenMediaPlaneKhrWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint plane = 0;
    cl_uint expectedPlane = 2;
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getImageInfo(CL_IMAGE_DX9_MEDIA_PLANE_KHR, sizeof(cl_uint), &plane, &retSize);
    EXPECT_EQ(expectedPlane, plane);
    EXPECT_EQ(sizeof(cl_uint), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, GivenPlaneIntelWhenGetMemObjInfoThenCorrectInfoIsReturned) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    cl_dx9_media_adapter_type_khr expectedAdapterType = 5;
    cl_uint plane = 0;
    cl_uint expectedPlane = 2;
    surfaceInfo.shared_handle = (HANDLE)1;
    size_t retSize = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE,
                                                               expectedAdapterType, expectedPlane, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());

    sharedImg->getImageInfo(CL_IMAGE_DX9_PLANE_INTEL, sizeof(cl_uint), &plane, &retSize);
    EXPECT_EQ(expectedPlane, plane);
    EXPECT_EQ(sizeof(cl_uint), retSize);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenSharedHandleWhenCreateThenDontCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)1;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_TRUE(surface->isSharedResource());
    EXPECT_EQ(nullptr, surface->getResourceStaging());

    EXPECT_EQ(0u, mockSharingFcns->createTexture2dCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->addRefCalled);
}

TEST_F(D3D9Tests, givenZeroSharedHandleAndLockableFlagWhenCreateThenDontCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_FALSE(surface->isSharedResource());
    EXPECT_EQ(nullptr, surface->getResourceStaging());
    EXPECT_TRUE(surface->lockable);

    EXPECT_EQ(0u, mockSharingFcns->createTexture2dCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->addRefCalled);
}

TEST_F(D3D9Tests, givenZeroSharedHandleAndNonLockableFlagWhenCreateThenCreateStagingSurface) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = D3DResourceFlags::USAGE_RENDERTARGET;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    mockSharingFcns->createTexture2dSetParams = true;
    mockSharingFcns->createTexture2dParamsSet.texture = (D3DTexture2d *)&dummyD3DSurfaceStaging;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);

    auto surface = static_cast<D3DSurface *>(sharedImg->getSharingHandler().get());
    EXPECT_FALSE(surface->isSharedResource());
    EXPECT_NE(nullptr, surface->getResourceStaging());
    EXPECT_FALSE(surface->lockable);

    EXPECT_EQ(1u, mockSharingFcns->createTexture2dCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->addRefCalled);
}

TEST_F(D3D9Tests, GivenSharedResourceSurfaceAndEnabledInteropUserSyncWhenReleasingThenResourcesAreReleased) {
    context->setInteropUserSyncEnabled(true);
    surfaceInfo.shared_handle = (HANDLE)1;
    mockGmmResInfo->cpuBltCalled = 0u;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg);
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    EXPECT_EQ(0u, mockGmmResInfo->arrayIndexPassedToGetOffset);
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memoryManager->lockResourceCalled);
    EXPECT_EQ(0, memoryManager->unlockResourceCalled);
    EXPECT_EQ(0u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(0u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(0u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, GivenSharedResourceSurfaceAndDisabledInteropUserSyncWhenReleasingThenResourcesAreReleased) {
    context->setInteropUserSyncEnabled(false);
    surfaceInfo.shared_handle = (HANDLE)1;

    mockGmmResInfo->cpuBltCalled = 0u;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(1u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memoryManager->lockResourceCalled);
    EXPECT_EQ(0, memoryManager->unlockResourceCalled);
    EXPECT_EQ(0u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(0u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, GivenSharedResourceSurfaceAndDisabledInteropUserSyncIntelWhenReleasingThenResourcesAreReleased) {
    context->setInteropUserSyncEnabled(false);
    surfaceInfo.shared_handle = (HANDLE)1;

    mockGmmResInfo->cpuBltCalled = 0u;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    cl_mem clMem = sharedImg.get();

    auto retVal = clEnqueueAcquireDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memoryManager->lockResourceCalled);
    EXPECT_EQ(0, memoryManager->unlockResourceCalled);
    EXPECT_EQ(0u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(0u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(0u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, GivenNonSharedResourceSurfaceAndLockableWhenReleasingThenResourcesAreReleased) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    mockSharingFcns->lockRectSetParams = true;
    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    memoryManager->lockResourceReturnValue = returnedLockedRes;
    memoryManager->expectedLockingAllocation = graphicsAllocation;

    GMM_RES_COPY_BLT &requestedResCopyBlt = mockGmmResInfo->requestedResCopyBlt;
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    mockGmmResInfo->cpuBltCalled = 0u;
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1, memoryManager->lockResourceCalled);
    EXPECT_EQ(1, memoryManager->unlockResourceCalled);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(1u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->lockRectCalled);

    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    requestedResCopyBlt = {};
    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_EQ(1u, mockSharingFcns->unlockRectCalled);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(2, memoryManager->lockResourceCalled);
    EXPECT_EQ(2, memoryManager->unlockResourceCalled);
    EXPECT_EQ(2u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(2u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(2u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(0u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->lockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(static_cast<uint32_t>(D3DLOCK_READONLY), mockSharingFcns->lockRectParamsPassed[0].flags);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->lockRectParamsPassed[1].d3dResource);
    EXPECT_EQ(0u, mockSharingFcns->lockRectParamsPassed[1].flags);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->unlockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->unlockRectParamsPassed[1].d3dResource);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, GivenNonSharedResourceSurfaceAndLockableIntelWhenReleasingThenResourcesAreReleased) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = 0;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    mockSharingFcns->lockRectSetParams = true;
    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    memoryManager->lockResourceReturnValue = returnedLockedRes;
    memoryManager->expectedLockingAllocation = graphicsAllocation;

    GMM_RES_COPY_BLT &requestedResCopyBlt = mockGmmResInfo->requestedResCopyBlt;
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    mockGmmResInfo->cpuBltCalled = 0u;
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    auto retVal = clEnqueueAcquireDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(1, memoryManager->lockResourceCalled);
    EXPECT_EQ(1, memoryManager->unlockResourceCalled);
    EXPECT_EQ(1u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->lockRectCalled);

    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    requestedResCopyBlt = {};
    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_EQ(1u, mockSharingFcns->unlockRectCalled);

    retVal = clEnqueueReleaseDX9ObjectsINTEL(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(2, memoryManager->lockResourceCalled);
    EXPECT_EQ(2, memoryManager->unlockResourceCalled);
    EXPECT_EQ(2u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(2u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(2u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(0u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(0u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->lockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(static_cast<uint32_t>(D3DLOCK_READONLY), mockSharingFcns->lockRectParamsPassed[0].flags);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->lockRectParamsPassed[1].d3dResource);
    EXPECT_EQ(0u, mockSharingFcns->lockRectParamsPassed[1].flags);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->unlockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->unlockRectParamsPassed[1].d3dResource);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, GivenNonSharedResourceSurfaceAndNonLockableWhenReleasingThenResourcesAreReleased) {
    surfaceInfo.shared_handle = (HANDLE)0;
    mockSharingFcns->mockTexture2dDesc.Usage = D3DResourceFlags::USAGE_RENDERTARGET;
    D3DLOCKED_RECT lockedRect = {10u, (void *)100};

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    mockSharingFcns->createTexture2dSetParams = true;
    mockSharingFcns->createTexture2dParamsSet.texture = (D3DTexture2d *)&dummyD3DSurfaceStaging;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, mockGmmResInfo->getOffsetCalled);
    cl_mem clMem = sharedImg.get();
    auto imgHeight = static_cast<ULONG>(sharedImg->getImageDesc().image_height);
    void *returnedLockedRes = (void *)100;

    mockSharingFcns->lockRectSetParams = true;
    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    memoryManager->lockResourceReturnValue = returnedLockedRes;
    memoryManager->expectedLockingAllocation = graphicsAllocation;

    GMM_RES_COPY_BLT &requestedResCopyBlt = mockGmmResInfo->requestedResCopyBlt;
    mockGmmResInfo->cpuBltCalled = 0u;
    GMM_RES_COPY_BLT expectedResCopyBlt = {};
    expectedResCopyBlt.Sys.pData = lockedRect.pBits;
    expectedResCopyBlt.Gpu.pData = returnedLockedRes;
    expectedResCopyBlt.Sys.RowPitch = lockedRect.Pitch;
    expectedResCopyBlt.Blt.Upload = 1;
    expectedResCopyBlt.Sys.BufferSize = lockedRect.Pitch * imgHeight;

    auto retVal = clEnqueueAcquireDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(1, memoryManager->lockResourceCalled);
    EXPECT_EQ(1, memoryManager->unlockResourceCalled);
    EXPECT_EQ(1u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->lockRectCalled);

    mockSharingFcns->lockRectParamsSet.lockedRect = lockedRect;

    expectedResCopyBlt.Blt.Upload = 0;

    EXPECT_EQ(1u, mockSharingFcns->unlockRectCalled);

    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &clMem, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(memcmp(&requestedResCopyBlt, &expectedResCopyBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(2, memoryManager->lockResourceCalled);
    EXPECT_EQ(2, memoryManager->unlockResourceCalled);
    EXPECT_EQ(2u, mockGmmResInfo->cpuBltCalled);
    EXPECT_EQ(1u, mockSharingFcns->createTexture2dCalled);
    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
    EXPECT_EQ(1u, mockSharingFcns->flushAndWaitCalled);
    EXPECT_EQ(2u, mockSharingFcns->lockRectCalled);
    EXPECT_EQ(2u, mockSharingFcns->unlockRectCalled);
    EXPECT_EQ(1u, mockSharingFcns->getRenderTargetDataCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateSurfaceCalled);
    EXPECT_EQ(1u, mockSharingFcns->updateDeviceCalled);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->lockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(static_cast<uint32_t>(D3DLOCK_READONLY), mockSharingFcns->lockRectParamsPassed[0].flags);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->lockRectParamsPassed[1].d3dResource);
    EXPECT_EQ(0u, mockSharingFcns->lockRectParamsPassed[1].flags);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->unlockRectParamsPassed[0].d3dResource);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->unlockRectParamsPassed[1].d3dResource);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->getRenderTargetDataParamsPassed[0].renderTarget);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->getRenderTargetDataParamsPassed[0].dstSurface);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurfaceStaging), mockSharingFcns->updateSurfaceParamsPassed[0].renderTarget);
    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateSurfaceParamsPassed[0].dstSurface);

    EXPECT_EQ(reinterpret_cast<IDirect3DSurface9 *>(&dummyD3DSurface), mockSharingFcns->updateDeviceParamsPassed[0].resource);
}

TEST_F(D3D9Tests, givenInvalidClMemObjectPassedOnReleaseListWhenCallIsMadeThenFailureIsReturned) {
    auto fakeObject = reinterpret_cast<cl_mem>(cmdQ);
    auto retVal = clEnqueueReleaseDX9MediaSurfacesKHR(cmdQ, 1, &fakeObject, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(D3D9Tests, givenInvalidClMemObjectPassedOnReleaseDX9ObjectsWhenCallIsMadeThenFailureIsReturned) {
    auto fakeObject = reinterpret_cast<cl_mem>(cmdQ);
    auto retVal = clEnqueueReleaseDX9ObjectsINTEL(cmdQ, 1, &fakeObject, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(D3D9Tests, givenResourcesCreatedFromDifferentDevicesWhenAcquireReleaseCalledThenUpdateDevice) {
    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto createdResourceDevice = reinterpret_cast<D3DDevice *>(123);
    mockSharingFcns->setDevice(createdResourceDevice); // create call will pick this device
    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    memoryManager->expectedLockingAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());

    mockSharingFcns->setDevice(nullptr); // force device change
    sharedImg->getSharingHandler()->acquire(sharedImg.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(createdResourceDevice, mockSharingFcns->getDevice());

    mockSharingFcns->setDevice(nullptr); // force device change
    sharedImg->getSharingHandler()->release(sharedImg.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(createdResourceDevice, mockSharingFcns->getDevice());

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, givenNullD3dDeviceWhenContextIsCreatedThenReturnErrorOnSurfaceCreation) {
    cl_device_id deviceID = context->getDevice(0);
    cl_int retVal = CL_SUCCESS;

    cl_context_properties properties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)pPlatform,
                                           CL_CONTEXT_ADAPTER_D3D9_KHR, 0, 0};

    std::unique_ptr<MockContext> ctx(Context::create<MockContext>(properties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>()->getDevice());

    auto img = D3DSurface::create(ctx.get(), nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenInvalidContextWhenSurfaceIsCreatedThenReturnError) {
    cl_device_id deviceID = context->getDevice(0);
    cl_int retVal = CL_SUCCESS;

    cl_context_properties properties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)(cl_platform_id)pPlatform, 0};

    std::unique_ptr<MockContext> ctx(Context::create<MockContext>(properties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, ctx->getSharing<D3DSharingFunctions<D3D9>>());

    auto img = D3DSurface::create(ctx.get(), nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);

    img = D3DSurface::create(nullptr, nullptr, CL_MEM_READ_WRITE, 0, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenInvalidFlagsWhenSurfaceIsCreatedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    auto img = clCreateFromDX9MediaSurfaceINTEL(context, CL_MEM_USE_HOST_PTR, surfaceInfo.resource, surfaceInfo.shared_handle, 0, &retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenInvalidContextWhenImageIsCreatedThenErrorIsReturned) {
    auto invalidContext = reinterpret_cast<cl_context>(this->cmdQ);
    auto retVal = CL_SUCCESS;

    auto img = clCreateFromDX9MediaSurfaceINTEL(invalidContext, CL_MEM_READ_WRITE, surfaceInfo.resource, surfaceInfo.shared_handle, 0, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, img);
}

TEST_F(D3D9Tests, givenTheSameResourceAndPlaneWhenSurfaceIsCreatedThenReturnError) {
    mockSharingFcns->mockTexture2dDesc.Format = (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), code supplied by vendor
    surfaceInfo.shared_handle = (HANDLE)1;
    cl_int retVal = CL_SUCCESS;
    cl_uint plane = 0;

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, plane, &retVal));
    EXPECT_NE(nullptr, sharedImg.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto sharedImg2 = D3DSurface::create(context, &surfaceInfo, CL_MEM_READ_WRITE, 0, plane, &retVal);
    EXPECT_EQ(nullptr, sharedImg2);
    EXPECT_EQ(CL_INVALID_DX9_RESOURCE_INTEL, retVal);

    EXPECT_EQ(1u, mockSharingFcns->getTexture2dDescCalled);
}

TEST_F(D3D9Tests, WhenFillingBufferDescThenBufferContentsAreCorrect) {
    D3D9::D3DBufferDesc requestedDesc = {};
    D3D9::D3DBufferDesc expectedDesc = {};
    memset(&requestedDesc, 0, sizeof(requestedDesc));
    memset(&expectedDesc, 0, sizeof(expectedDesc));

    mockSharingFcns->fillCreateBufferDesc(requestedDesc, 10);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DBufferDesc)) == 0);
}

TEST_F(D3D9Tests, WhenFillingTexture2dDescThenTextureContentsAreCorrect) {
    D3D9::D3DTexture2dDesc requestedDesc = {};
    D3D9::D3DTexture2dDesc expectedDesc = {};
    D3D9::D3DTexture2dDesc srcDesc = {};
    cl_uint subresource = 4;
    memset(&requestedDesc, 0, sizeof(requestedDesc));
    memset(&expectedDesc, 0, sizeof(expectedDesc));
    mockSharingFcns->fillCreateTexture2dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DTexture2dDesc)) == 0);
}

TEST_F(D3D9Tests, WhenFillingTexture3dDescThenTextureContentsAreCorrect) {
    D3D9::D3DTexture3dDesc requestedDesc = {};
    D3D9::D3DTexture3dDesc expectedDesc = {};
    D3D9::D3DTexture3dDesc srcDesc = {};
    cl_uint subresource = 4;
    memset(&requestedDesc, 0, sizeof(requestedDesc));
    memset(&expectedDesc, 0, sizeof(expectedDesc));
    mockSharingFcns->fillCreateTexture3dDesc(requestedDesc, &srcDesc, subresource);
    EXPECT_TRUE(memcmp(&requestedDesc, &expectedDesc, sizeof(D3D9::D3DTexture3dDesc)) == 0);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromDX9ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromDX9INTEL(nullptr, 1, nullptr, 1, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireDX9ObjectsINTEL(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseDX9ObjectsINTEL(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromDX9MediaAdapterThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromDX9MediaAdapterKHR(nullptr, 1, nullptr, nullptr, 1, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9MediaSurfacesAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireDX9MediaSurfacesKHR(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenDX9MediaSurfacesAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseDX9MediaSurfacesKHR(nullptr, 1, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromD3D10ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromD3D10KHR(nullptr, 0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10BufferThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10BufferKHR(nullptr, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10Texture2DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10Texture2DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D10Texture3DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D10Texture3DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D10ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireD3D10ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D10ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseD3D10ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperPlatformWhenGettingDeviceIDsFromD3D11ThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clGetDeviceIDsFromD3D11KHR(nullptr, 0, nullptr, 0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11BufferThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11BufferKHR(nullptr, 0, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11Texture2DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11Texture2DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperContextWhenCreatingFromD3D11Texture3DThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    clCreateFromD3D11Texture3DKHR(nullptr, 0, nullptr, 0u, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D11ObjectsAreAcquiredThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueAcquireD3D11ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(D3D9Tests, givenImproperCommandQueueWhenD3D11ObjectsAreReleasedThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    retVal = clEnqueueReleaseD3D11ObjectsKHR(nullptr, 0, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

namespace D3D9Formats {
static const std::tuple<uint32_t /*d3dFormat*/, uint32_t /*plane*/, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, ImagePlane> allImageFormats[] = {
    // input, input, output, output
    std::make_tuple(D3DFMT_R32F, 0, CL_R, CL_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_R16F, 0, CL_R, CL_HALF_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_L16, 0, CL_R, CL_UNORM_INT16, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A8, 0, CL_A, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_L8, 0, CL_R, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_G32R32F, 0, CL_RG, CL_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_G16R16F, 0, CL_RG, CL_HALF_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_G16R16, 0, CL_RG, CL_UNORM_INT16, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A8L8, 0, CL_RG, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A32B32G32R32F, 0, CL_RGBA, CL_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A16B16G16R16F, 0, CL_RGBA, CL_HALF_FLOAT, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A16B16G16R16, 0, CL_RGBA, CL_UNORM_INT16, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A8B8G8R8, 0, CL_RGBA, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_X8B8G8R8, 0, CL_RGBA, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_A8R8G8B8, 0, CL_BGRA, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_X8R8G8B8, 0, CL_BGRA, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 0, CL_R, CL_UNORM_INT8, ImagePlane::planeY),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 1, CL_RG, CL_UNORM_INT8, ImagePlane::planeUV),
    std::make_tuple(MAKEFOURCC('N', 'V', '1', '2'), 2, 0, 0, ImagePlane::noPlane),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 0, CL_R, CL_UNORM_INT8, ImagePlane::planeY),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 1, CL_R, CL_UNORM_INT8, ImagePlane::planeV),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 2, CL_R, CL_UNORM_INT8, ImagePlane::planeU),
    std::make_tuple(MAKEFOURCC('Y', 'V', '1', '2'), 3, 0, 0, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_YUY2, 0, CL_YUYV_INTEL, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_UYVY, 0, CL_UYVY_INTEL, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(MAKEFOURCC('Y', 'V', 'Y', 'U'), 0, CL_YVYU_INTEL, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(MAKEFOURCC('V', 'Y', 'U', 'Y'), 0, CL_VYUY_INTEL, CL_UNORM_INT8, ImagePlane::noPlane),
    std::make_tuple(D3DFMT_UNKNOWN, 0, 0, 0, ImagePlane::noPlane)};
}

struct D3D9ImageFormatTests
    : public ::testing::WithParamInterface<std::tuple<uint32_t /*d3dFormat*/, uint32_t /*plane*/, uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, ImagePlane>>,
      public ::testing::Test {
};

INSTANTIATE_TEST_SUITE_P(
    D3D9ImageFormatTests,
    D3D9ImageFormatTests,
    testing::ValuesIn(D3D9Formats::allImageFormats));

TEST_P(D3D9ImageFormatTests, WhenGettingImageFormatThenValidFormatDetailsAreReturned) {
    cl_image_format imgFormat = {};
    auto format = std::get<0>(GetParam());
    auto plane = std::get<1>(GetParam());
    ImagePlane imagePlane = ImagePlane::noPlane;
    auto expectedImagePlane = std::get<4>(GetParam());
    auto expectedClChannelType = static_cast<cl_channel_type>(std::get<3>(GetParam()));
    auto expectedClChannelOrder = static_cast<cl_channel_order>(std::get<2>(GetParam()));

    D3DSurface::findImgFormat((D3DFORMAT)format, imgFormat, plane, imagePlane);

    EXPECT_EQ(imgFormat.image_channel_data_type, expectedClChannelType);
    EXPECT_EQ(imgFormat.image_channel_order, expectedClChannelOrder);
    EXPECT_TRUE(imagePlane == expectedImagePlane);
}

using D3D9MultiRootDeviceTest = MultiRootDeviceFixture;

TEST_F(D3D9MultiRootDeviceTest, givenD3DHandleIsNullWhenCreatingSharedSurfaceAndRootDeviceIndexIsSpecifiedThenAllocationHasCorrectRootDeviceIndex) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 10;
    imgDesc.imageWidth = 10;
    imgDesc.imageDepth = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto gmm = MockGmm::queryImgParams(device1->getGmmHelper(), imgInfo, false).release();

    auto memoryManager = std::make_unique<MockMM>(*device1->executionEnvironment);
    memoryManager->forceGmm = gmm;

    auto mockSharingFcns = new MockD3DSharingFunctions<D3DTypesHelper::D3D9>();
    mockSharingFcns->mockTexture2dDesc.Format = D3DFMT_R32F;
    mockSharingFcns->mockTexture2dDesc.Height = 10;
    mockSharingFcns->mockTexture2dDesc.Width = 10;

    cl_dx9_surface_info_khr surfaceInfo = {};
    surfaceInfo.shared_handle = reinterpret_cast<HANDLE>(0);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    MockContext ctx(device1);
    ctx.setSharingFunctions(mockSharingFcns);
    ctx.memoryManager = memoryManager.get();

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(&ctx, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(expectedRootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
}

TEST_F(D3D9MultiRootDeviceTest, givenD3DHandleIsNotNullWhenCreatingSharedSurfaceAndRootDeviceIndexIsSpecifiedThenAllocationHasCorrectRootDeviceIndex) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 10;
    imgDesc.imageWidth = 10;
    imgDesc.imageDepth = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto gmm = MockGmm::queryImgParams(device1->getGmmHelper(), imgInfo, false).release();

    auto memoryManager = std::make_unique<MockMM>(*device1->executionEnvironment);
    memoryManager->forceGmm = gmm;

    auto mockSharingFcns = new MockD3DSharingFunctions<D3DTypesHelper::D3D9>();
    mockSharingFcns->mockTexture2dDesc.Format = D3DFMT_R32F;
    mockSharingFcns->mockTexture2dDesc.Height = 10;
    mockSharingFcns->mockTexture2dDesc.Width = 10;

    cl_dx9_surface_info_khr surfaceInfo = {};
    surfaceInfo.shared_handle = reinterpret_cast<HANDLE>(1);

    mockSharingFcns->getTexture2dDescSetParams = true;
    mockSharingFcns->getTexture2dDescParamsSet.textureDesc = mockSharingFcns->mockTexture2dDesc;

    MockContext ctx(device1);
    ctx.setSharingFunctions(mockSharingFcns);
    ctx.memoryManager = memoryManager.get();

    auto sharedImg = std::unique_ptr<Image>(D3DSurface::create(&ctx, &surfaceInfo, CL_MEM_READ_WRITE, 0, 0, nullptr));
    ASSERT_NE(nullptr, sharedImg.get());
    auto graphicsAllocation = sharedImg->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(expectedRootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
}
} // namespace NEO
