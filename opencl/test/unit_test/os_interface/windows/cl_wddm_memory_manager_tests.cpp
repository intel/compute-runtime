/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_memory_manager_tests.h"

using namespace NEO;
using namespace ::testing;

ClWddmMemoryManagerFixture::ClWddmMemoryManagerFixture() : executionEnvironment(*platform()->peekExecutionEnvironment()) {
    executionEnvironment.incRefInternal();
}

ClWddmMemoryManagerFixture::~ClWddmMemoryManagerFixture() {
    executionEnvironment.decRefInternal();
}

void ClWddmMemoryManagerFixture::setUp() {
    GdiDllFixture::setUp();

    rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();
    wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    if (defaultHwInfo->capabilityTable.ftrRenderCompressedBuffers || defaultHwInfo->capabilityTable.ftrRenderCompressedImages) {
        GMM_TRANSLATIONTABLE_CALLBACKS dummyTTCallbacks = {};

        auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
        auto hwInfo = *defaultHwInfo;
        EngineInstancesContainer regularEngines = {
            {aub_stream::ENGINE_CCS, EngineUsage::regular}};

        memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                          PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

        for (auto engine : memoryManager->getRegisteredEngines(rootDeviceIndex)) {
            if (engine.getEngineUsage() == EngineUsage::regular) {
                engine.commandStreamReceiver->pageTableManager.reset(GmmPageTableMngr::create(nullptr, 0, &dummyTTCallbacks));
            }
        }
    }
    wddm->init();
    constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
    wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);

    rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);

    memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
}

HWTEST_F(ClWddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(ClWddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

HWTEST_F(ClWddmMemoryManagerTest, givenWddmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenallocateGraphicsMemoryForImageIsUsed) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), &executionEnvironment, 0u));
    MockContext context(device.get());
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    char data[64u * 64u * 4 * 8];

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(ClWddmMemoryManagerTest, givenWddmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryForImageIsUsed) {
    MockContext context;
    context.memoryManager = memoryManager.get();

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());

    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex());
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_EQ(GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE, imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage);
}

TEST_F(BufferWithWddmMemory, WhenCreatingBufferThenBufferIsCreatedCorrectly) {
    flags = CL_MEM_USE_HOST_PTR | CL_MEM_FORCE_HOST_MEMORY_INTEL;

    auto ptr = alignedMalloc(MemoryConstants::preferredAlignment, MemoryConstants::preferredAlignment);

    auto buffer = Buffer::create(
        &context,
        flags,
        MemoryConstants::preferredAlignment,
        ptr,
        retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, buffer);

    auto address = buffer->getCpuAddress();
    if (buffer->isMemObjZeroCopy()) {
        EXPECT_EQ(ptr, address);
    } else {
        EXPECT_NE(address, ptr);
    }
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex()));
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer());

    delete buffer;
    alignedFree(ptr);
}

TEST_F(BufferWithWddmMemory, GivenNullOsHandleStorageWhenPopulatingThenFilledPointerIsReturned) {
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    memoryManager->populateOsHandles(storage, 0);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, static_cast<OsHandleWin *>(storage.fragmentStorageData[0].osHandleStorage)->gmm);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, 0);
}

TEST_F(BufferWithWddmMemory, GivenMisalignedHostPtrAndMultiplePagesSizeWhenAskedForGraphicsAllocationThenItContainsAllFragmentsWithProperGpuAdrresses) {
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize * 10;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), false, size, context.getDevice(0)->getDeviceBitfield()}, ptr);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    ASSERT_EQ(3u, hostPtrManager->getFragmentCount());

    auto reqs = MockHostPtrManager::getAllocationRequirements(context.getDevice(0)->getRootDeviceIndex(), ptr, size);

    for (int i = 0; i < maxFragmentsCount; i++) {
        auto osHandle = static_cast<OsHandleWin *>(graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_NE(0u, osHandle->handle);

        EXPECT_NE(nullptr, osHandle->gmm);
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr,
                  reinterpret_cast<void *>(osHandle->gmm->resourceParams.pExistingSysMem));
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize,
                  osHandle->gmm->resourceParams.BaseWidth);
    }
    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(BufferWithWddmMemory, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;

    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1000);
    auto ptr2 = reinterpret_cast<void *>(wddm->virtualAllocAddress + 0x1001);
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedThenGraphicsAddressIsPopulatedFromProperFragment) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAddress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;
    auto maxOsContextCount = 1u;

    auto osHandle = new OsHandleWin();

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = osHandle;
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    osHandle->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    osHandle->gpuPtr = gpuAddress;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragment);

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, allocation->getGpuAddress());
    EXPECT_EQ(0ULL, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(BufferWithWddmMemory, givenFragmentsThatAreNotInOrderWhenGraphicsAllocationIsBeingCreatedNotAllignedToPageThenGraphicsAddressIsPopulatedFromProperFragmentAndOffsetisAssigned) {
    memoryManager->setForce32bitAllocations(true);
    OsHandleStorage handleStorage = {};
    D3DGPU_VIRTUAL_ADDRESS gpuAddress = MemoryConstants::pageSize * 1;
    auto ptr = reinterpret_cast<void *>(wddm->virtualAllocAddress + MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize * 2;
    auto maxOsContextCount = 1u;

    auto osHandle = new OsHandleWin();

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[0].osHandleStorage = osHandle;
    handleStorage.fragmentStorageData[0].residency = new ResidencyData(maxOsContextCount);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    osHandle->gmm = new Gmm(rootDeviceEnvironment->getGmmHelper(), ptr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    handleStorage.fragmentCount = 1;

    FragmentStorage fragment = {};
    fragment.driverAllocation = true;
    fragment.fragmentCpuPointer = ptr;
    fragment.fragmentSize = size;
    fragment.osInternalStorage = handleStorage.fragmentStorageData[0].osHandleStorage;
    osHandle->gpuPtr = gpuAddress;
    memoryManager->getHostPtrManager()->storeFragment(rootDeviceIndex, fragment);

    auto offset = 80u;
    auto allocationPtr = ptrOffset(ptr, offset);
    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = allocationPtr;
    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, allocationData);

    EXPECT_EQ(allocationPtr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress + offset, allocation->getGpuAddress()); // getGpuAddress returns gpuAddress + allocationOffset
    EXPECT_EQ(offset, allocation->getAllocationOffset());
    memoryManager->freeGraphicsMemory(allocation);
}

struct ClWddmMemoryManagerWithAsyncDeleterTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        executionEnvironment->incRefInternal();
        wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        wddm->resetGdi(new MockGdi());
        wddm->callBaseDestroyAllocations = false;
        wddm->init();
        deleter = new MockDeferredDeleter;
        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);
        memoryManager->setDeferredDeleter(deleter);
    }

    void TearDown() override {
        executionEnvironment->decRefInternal();
    }

    MockDeferredDeleter *deleter = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    ExecutionEnvironment *executionEnvironment;
    HardwareInfo *hwInfo;
    WddmMock *wddm;
};

TEST_F(ClWddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenDrainIsCalledAndCreateAllocationIsCalledTwice) {
    UltDeviceFactory deviceFactory{1, 0};
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    deleter->expectDrainBlockingValue(true);
    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1, deleter->drainCalled);
    EXPECT_EQ(2u, wddm->createAllocationResult.called);
}

TEST_F(ClWddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithAsyncDeleterWhenCanAllocateMemoryForTiledImageThenDrainIsNotCalledAndCreateAllocationIsCalledOnce) {
    UltDeviceFactory deviceFactory{1, 0};
    ImageDescriptor imgDesc;
    imgDesc.imageType = ImageType::image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_SUCCESS;
    wddm->mapGpuVaStatus = true;
    wddm->callBaseMapGpuVa = false;
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, wddm->mapGpuVirtualAddressResult.called);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    auto allocation = memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(0, deleter->drainCalled);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(1u, wddm->mapGpuVirtualAddressResult.called);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(ClWddmMemoryManagerWithAsyncDeleterTest, givenMemoryManagerWithoutAsyncDeleterWhenCannotAllocateMemoryForTiledImageThenCreateAllocationIsCalledOnce) {
    UltDeviceFactory deviceFactory{1, 0};
    memoryManager->setDeferredDeleter(nullptr);
    ImageDescriptor imgDesc;
    imgDesc.imageType = ImageType::image3D;
    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    wddm->createAllocationStatus = STATUS_GRAPHICS_NO_VIDEO_MEMORY;
    EXPECT_EQ(0u, wddm->createAllocationResult.called);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, deviceFactory.rootDevices[0]);
    AllocationProperties allocProperties = MemObjHelper::getAllocationPropertiesWithImageInfo(0, imgInfo, true, memoryProperties, *hwInfo, mockDeviceBitfield, true);

    memoryManager->allocateGraphicsMemoryInPreferredPool(allocProperties, nullptr);
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
}

struct PlatformWithFourDevicesTest : public ::testing::Test {
    PlatformWithFourDevicesTest() {
        ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    }
    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(4);
        initPlatform();
    }

    DebugManagerStateRestore restorer;

    VariableBackup<UltHwConfig> backup{&ultHwConfig};
};

TEST_F(PlatformWithFourDevicesTest, whenCreateColoredAllocationAndWddmReturnsCanonizedAddressDuringMapingVAThenAddressIsBeingDecanonizedAndAbortIsNotThrownFromUnrecoverableIfStatement) {
    struct CanonizeAddressMockWddm : public WddmMock {
        using WddmMock::WddmMock;

        bool mapGpuVirtualAddress(Gmm *gmm, D3DKMT_HANDLE handle, D3DGPU_VIRTUAL_ADDRESS minimumAddress, D3DGPU_VIRTUAL_ADDRESS maximumAddress, D3DGPU_VIRTUAL_ADDRESS preferredAddress, D3DGPU_VIRTUAL_ADDRESS &gpuPtr, AllocationType type) override {
            auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
            gpuPtr = gmmHelper->canonize(preferredAddress);
            return mapGpuVaStatus;
        }
    };

    auto wddm = new CanonizeAddressMockWddm(*platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]);
    wddm->init();
    auto osInterfaceMock = new OSInterface();
    auto callBaseDestroyBackup = wddm->callBaseDestroyAllocations;
    wddm->callBaseDestroyAllocations = false;
    wddm->mapGpuVaStatus = true;
    osInterfaceMock->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    auto osInterfaceBackUp = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceMock);

    MockWddmMemoryManager memoryManager(true, true, *platform()->peekExecutionEnvironment());
    memoryManager.supportsMultiStorageResources = true;

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrMultiTileArch = true;

    GraphicsAllocation *allocation = nullptr;
    EXPECT_NO_THROW(allocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, 4 * MemoryConstants::pageSize64k, AllocationType::buffer, true, mockDeviceBitfield}));
    EXPECT_NE(nullptr, allocation);

    memoryManager.freeGraphicsMemory(allocation);
    wddm->callBaseDestroyAllocations = callBaseDestroyBackup;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceBackUp);
}

TEST_F(PlatformWithFourDevicesTest, givenDifferentAllocationSizesWhenColourAllocationThenResourceIsSpreadProperly) {
    auto wddm = reinterpret_cast<WddmMock *>(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->mapGpuVaStatus = true;
    VariableBackup<bool> restorer{&wddm->callBaseMapGpuVa, false};

    MockWddmMemoryManager memoryManager(true, true, *platform()->peekExecutionEnvironment());
    memoryManager.supportsMultiStorageResources = true;

    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->featureTable.flags.ftrMultiTileArch = true;

    // We are allocating memory from 4 to 12 pages and want to check if remainders (1, 2 or 3 pages in case of 4 devices) are spread equally.
    for (int additionalSize = 0; additionalSize <= 8; additionalSize++) {
        auto allocation = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, (4 + additionalSize) * MemoryConstants::pageSize64k, AllocationType::buffer, true, 0b1111}));
        auto handles = allocation->getNumGmms();

        EXPECT_EQ(4u, handles);
        auto size = allocation->getAlignedSize() / MemoryConstants::pageSize64k;
        switch (size % handles) {
        case 0:
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 1:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 2:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
            break;
        case 3:
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(1)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ((size / handles + 1) * MemoryConstants::pageSize64k, allocation->getGmm(2)->gmmResourceInfo->getSizeAllocation());
            EXPECT_EQ(size / handles * MemoryConstants::pageSize64k, allocation->getGmm(3)->gmmResourceInfo->getSizeAllocation());
        default:
            break;
        }
        memoryManager.freeGraphicsMemory(allocation);
    }
}

TEST_F(PlatformWithFourDevicesTest, whenCreateScratchSpaceInSingleTileQueueThenTheAllocationHasOneHandle) {
    MemoryManagerCreate<WddmMemoryManager> memoryManager(true, true, *platform()->peekExecutionEnvironment());

    AllocationProperties properties{mockRootDeviceIndex, true, 1u, AllocationType::scratchSurface, false, false, mockDeviceBitfield};
    auto allocation = static_cast<WddmAllocation *>(memoryManager.allocateGraphicsMemoryWithProperties(properties));
    EXPECT_EQ(1u, allocation->getNumGmms());
    memoryManager.freeGraphicsMemory(allocation);
}
