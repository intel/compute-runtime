/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "test.h"

using namespace NEO;

class ImageInLocalMemoryTest : public testing::Test {
  public:
    ImageInLocalMemoryTest() = default;

  protected:
    void SetUp() override {
        HardwareInfo inputPlatformDevice = *defaultHwInfo;
        inputPlatformDevice.featureTable.ftrLocalMemory = true;

        platformsImpl->clear();
        auto executionEnvironment = constructPlatform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&inputPlatformDevice);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        gmockMemoryManager = new ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation>(true, *executionEnvironment);
        executionEnvironment->memoryManager.reset(gmockMemoryManager);

        ON_CALL(*gmockMemoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(gmockMemoryManager, &GMockMemoryManagerFailFirstAllocation::baseAllocateGraphicsMemoryInDevicePool));

        device = std::make_unique<MockClDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));
        context = std::make_unique<MockContext>(device.get());

        imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
        imageDesc.image_width = 1;
        imageDesc.image_height = 1;
        imageDesc.image_row_pitch = sizeof(memory);

        imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        imageFormat.image_channel_order = CL_R;
    }

    void TearDown() override {}

    ::testing::NiceMock<GMockMemoryManagerFailFirstAllocation> *gmockMemoryManager = nullptr;
    cl_image_desc imageDesc{};
    cl_image_format imageFormat = {};
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    char memory[10];
};

TEST_F(ImageInLocalMemoryTest, givenImageWithoutHostPtrWhenLocalMemoryIsEnabledThenImageAllocationIsInLocalMemoryAndGpuAddressIsInStandard64KHeap) {

    cl_int retVal = 0;
    cl_mem_flags flags = CL_MEM_READ_WRITE;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryInDevicePool(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke(gmockMemoryManager, &GMockMemoryManagerFailFirstAllocation::baseAllocateGraphicsMemoryInDevicePool));

    std::unique_ptr<Image> image(Image::create(
        context.get(), MemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, memory, retVal));

    ASSERT_NE(nullptr, image);
    auto imgGfxAlloc = image->getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, imgGfxAlloc);
    EXPECT_EQ(MemoryPool::LocalMemory, imgGfxAlloc->getMemoryPool());
    EXPECT_LE(imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes, imgGfxAlloc->getUnderlyingBufferSize());
    EXPECT_EQ(GraphicsAllocation::AllocationType::IMAGE, imgGfxAlloc->getAllocationType());
    EXPECT_FALSE(imgGfxAlloc->getDefaultGmm()->useSystemMemoryPool);
    EXPECT_LT(GmmHelper::canonize(gmockMemoryManager->getGfxPartition(imgGfxAlloc->getRootDeviceIndex())->getHeapBase(HeapIndex::HEAP_STANDARD64KB)), imgGfxAlloc->getGpuAddress());
    EXPECT_GT(GmmHelper::canonize(gmockMemoryManager->getGfxPartition(imgGfxAlloc->getRootDeviceIndex())->getHeapLimit(HeapIndex::HEAP_STANDARD64KB)), imgGfxAlloc->getGpuAddress());
    EXPECT_EQ(0llu, imgGfxAlloc->getGpuBaseAddress());
}
