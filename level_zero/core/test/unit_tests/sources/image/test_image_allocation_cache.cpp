/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_reuse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_reuse_cleaner.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/image/image_allocation_cache.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <functional>

namespace L0 {
namespace ult {

struct MockImageAllocationCache : public ImageAllocationCache {
    using ImageAllocationCache::allocations;
    using ImageAllocationCache::ImageAllocationCache;
};

static const NEO::SurfaceFormatInfo firstSurfaceFormat = {};
static const NEO::SurfaceFormatInfo secondSurfaceFormat = {};

static ImageAllocationCacheKey createImageAllocationCacheKey(size_t width) {
    ImageAllocationCacheKey key = {};
    key.imgInfo.imgDesc.imageType = NEO::ImageType::image2D;
    key.imgInfo.imgDesc.imageWidth = width;
    key.imgInfo.imgDesc.imageHeight = 16u;
    key.imgInfo.imgDesc.imageDepth = 1u;
    key.imgInfo.surfaceFormat = &firstSurfaceFormat;
    key.deviceBitfield = NEO::DeviceBitfield(1u);
    key.allocationType = NEO::AllocationType::image;
    return key;
}

TEST(ImageAllocationCacheKeyTest, givenKeysDifferingInOneFieldWhenMatchingThenOnlyIdenticalKeysMatch) {
    const auto baseKey = createImageAllocationCacheKey(16u);
    EXPECT_TRUE(baseKey.matches(baseKey));

    const std::vector<std::function<void(ImageAllocationCacheKey &)>> modifiers = {
        [](ImageAllocationCacheKey &key) { key.rootDeviceIndex = 1u; },
        [](ImageAllocationCacheKey &key) { key.deviceBitfield = NEO::DeviceBitfield(2u); },
        [](ImageAllocationCacheKey &key) { key.allocationType = NEO::AllocationType::sharedImage; },
        [](ImageAllocationCacheKey &key) { key.preferCompressed = true; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageType = NEO::ImageType::image3D; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageWidth++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageHeight++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageDepth++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageArraySize++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageRowPitch++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.imageSlicePitch++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.numMipLevels++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.numSamples++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.imgDesc.fromParent = true; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.surfaceFormat = &secondSurfaceFormat; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.plane = NEO::ImagePlane::planeY; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.mipCount++; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.linearStorage = true; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.useLocalMemory = true; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.isDisplayable = true; },
        [](ImageAllocationCacheKey &key) { key.imgInfo.forceTiling = NEO::ImageTilingMode::tiledY; },
    };

    for (const auto &modifier : modifiers) {
        auto modifiedKey = baseKey;
        modifier(modifiedKey);
        EXPECT_FALSE(baseKey.matches(modifiedKey));
        EXPECT_FALSE(modifiedKey.matches(baseKey));
    }
}

struct ImageAllocationCacheFixture : public DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager> {
    void setUp() {
        DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager>::setUp();
        this->usmReuseInfo = &this->neoDevice->usmReuseInfo;
        this->usmReuseInfo->init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->memoryManager->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->cache = std::make_unique<MockImageAllocationCache>(*this->neoDevice, nullptr);
    }

    void tearDown() {
        this->cache.reset();
        DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager>::tearDown();
    }

    NEO::GraphicsAllocation *allocate() {
        return this->memoryManager->allocateGraphicsMemoryWithProperties(NEO::MockAllocationProperties{this->neoDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    }

    NEO::UsmReuseInfo *usmReuseInfo = nullptr;
    std::unique_ptr<MockImageAllocationCache> cache;
};

using ImageAllocationCacheTest = Test<ImageAllocationCacheFixture>;

TEST_F(ImageAllocationCacheTest, givenNullAllocationWhenInsertingThenItIsRejected) {
    EXPECT_FALSE(this->cache->insert(createImageAllocationCacheKey(16u), {}, nullptr));
    EXPECT_TRUE(this->cache->isEmpty());
}

TEST_F(ImageAllocationCacheTest, givenAllocationExceedingReuseBudgetWhenInsertingThenItIsRejected) {
    this->usmReuseInfo->init(MemoryConstants::pageSize - 1u, NEO::UsmReuseInfo::notLimited);
    auto allocation = this->allocate();

    EXPECT_FALSE(this->cache->insert(createImageAllocationCacheKey(16u), {}, allocation));
    EXPECT_TRUE(this->cache->isEmpty());
    EXPECT_EQ(0u, this->usmReuseInfo->getAllocationsSavedForReuseSize());

    this->memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(ImageAllocationCacheTest, givenCachedAllocationWhenGettingWithMatchingKeyThenAllocationAndImageInfoAreReturnedAndBudgetIsReleased) {
    auto allocation = this->allocate();
    NEO::ImageInfo cachedImgInfo = {};
    cachedImgInfo.size = 1234u;
    cachedImgInfo.rowPitch = 64u;
    cachedImgInfo.qPitch = 8u;

    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), cachedImgInfo, allocation));
    EXPECT_FALSE(this->cache->isEmpty());
    EXPECT_EQ(allocation->getUnderlyingBufferSize(), this->usmReuseInfo->getAllocationsSavedForReuseSize());

    NEO::ImageInfo imgInfo = {};
    EXPECT_EQ(allocation, this->cache->get(createImageAllocationCacheKey(16u), imgInfo));
    EXPECT_EQ(cachedImgInfo.size, imgInfo.size);
    EXPECT_EQ(cachedImgInfo.rowPitch, imgInfo.rowPitch);
    EXPECT_EQ(cachedImgInfo.qPitch, imgInfo.qPitch);
    EXPECT_TRUE(this->cache->isEmpty());
    EXPECT_EQ(0u, this->usmReuseInfo->getAllocationsSavedForReuseSize());

    this->memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(ImageAllocationCacheTest, givenCachedAllocationWhenGettingWithDifferentKeyThenNullptrIsReturned) {
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, this->allocate()));

    NEO::ImageInfo imgInfo = {};
    EXPECT_EQ(nullptr, this->cache->get(createImageAllocationCacheKey(32u), imgInfo));
    EXPECT_FALSE(this->cache->isEmpty());
}

TEST_F(ImageAllocationCacheTest, givenEmptyCacheWhenGettingThenNullptrIsReturnedWithoutInUseCheck) {
    NEO::ImageInfo imgInfo = {};
    EXPECT_EQ(nullptr, this->cache->get(createImageAllocationCacheKey(16u), imgInfo));
    EXPECT_EQ(0u, this->memoryManager->allocInUseCalled);
}

TEST_F(ImageAllocationCacheTest, givenCachedAllocationInUseWhenGettingThenItIsSkippedUntilCompleted) {
    auto allocation = this->allocate();
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, allocation));

    NEO::ImageInfo imgInfo = {};
    this->memoryManager->deferAllocInUse = true;
    EXPECT_EQ(nullptr, this->cache->get(createImageAllocationCacheKey(16u), imgInfo));
    EXPECT_FALSE(this->cache->isEmpty());

    this->memoryManager->deferAllocInUse = false;
    EXPECT_EQ(allocation, this->cache->get(createImageAllocationCacheKey(16u), imgInfo));

    this->memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(ImageAllocationCacheTest, givenCachedAllocationsWhenTrimmingThenAllAreFreed) {
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, this->allocate()));
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(32u), {}, this->allocate()));
    const auto freeCountBefore = this->memoryManager->freeGraphicsMemoryCalled.load();

    this->cache->trim();

    EXPECT_EQ(freeCountBefore + 2u, this->memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(this->cache->isEmpty());
    EXPECT_EQ(0u, this->usmReuseInfo->getAllocationsSavedForReuseSize());
}

TEST_F(ImageAllocationCacheTest, givenCleanedUpCacheWhenInsertingThenAllocationIsRejectedAndCachedAllocationsAreFreed) {
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, this->allocate()));
    const auto freeCountBefore = this->memoryManager->freeGraphicsMemoryCalled.load();

    this->cache->cleanup();
    EXPECT_EQ(freeCountBefore + 1u, this->memoryManager->freeGraphicsMemoryCalled);
    EXPECT_TRUE(this->cache->isEmpty());

    auto allocation = this->allocate();
    EXPECT_FALSE(this->cache->insert(createImageAllocationCacheKey(16u), {}, allocation));
    this->memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(ImageAllocationCacheTest, givenOldAndNewAllocationsWhenTrimmingOldAllocationsThenOnlyOldOnesAreFreedOnePerPassUnlessTrimAll) {
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, this->allocate()));
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(32u), {}, this->allocate()));
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(48u), {}, this->allocate()));
    const auto now = std::chrono::high_resolution_clock::now();
    this->cache->allocations[0].saveTime = now - std::chrono::seconds(30);
    this->cache->allocations[1].saveTime = now - std::chrono::seconds(20);
    this->cache->allocations[2].saveTime = now;
    const auto trimTimePoint = now - std::chrono::seconds(10);

    this->cache->trimOldAllocs(trimTimePoint, false);
    ASSERT_EQ(2u, this->cache->allocations.size());
    EXPECT_EQ(32u, this->cache->allocations[0].key.imgInfo.imgDesc.imageWidth);

    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(64u), {}, this->allocate()));
    this->cache->allocations[1].saveTime = now - std::chrono::seconds(15);
    this->cache->allocations[2].saveTime = now;

    this->cache->trimOldAllocs(trimTimePoint, true);
    ASSERT_EQ(1u, this->cache->allocations.size());
    EXPECT_EQ(64u, this->cache->allocations[0].key.imgInfo.imgDesc.imageWidth);
    EXPECT_FALSE(this->cache->isEmpty());
}

TEST_F(ImageAllocationCacheTest, givenOldAllocationInUseWhenTrimmingOldAllocationsThenItIsNotFreed) {
    EXPECT_TRUE(this->cache->insert(createImageAllocationCacheKey(16u), {}, this->allocate()));
    const auto now = std::chrono::high_resolution_clock::now();
    this->cache->allocations[0].saveTime = now - std::chrono::seconds(30);

    this->memoryManager->deferAllocInUse = true;
    this->cache->trimOldAllocs(now, true);
    EXPECT_EQ(1u, this->cache->allocations.size());

    this->memoryManager->deferAllocInUse = false;
    this->cache->trimOldAllocs(now, true);
    EXPECT_TRUE(this->cache->isEmpty());
}

TEST_F(ImageAllocationCacheTest, givenCacheWithReuseCleanerWhenInsertingThenCleanerThreadIsStartedAndCleanupUnregistersCache) {
    NEO::MockUnifiedMemoryReuseCleaner reuseCleaner(false);
    MockImageAllocationCache cache(*this->neoDevice, &reuseCleaner);
    reuseCleaner.registerAllocationCache(&cache);

    EXPECT_TRUE(cache.insert(createImageAllocationCacheKey(16u), {}, this->allocate()));
    EXPECT_TRUE(reuseCleaner.startThreadCalled);
    EXPECT_FALSE(reuseCleaner.isEmpty());

    cache.cleanup();
    EXPECT_TRUE(reuseCleaner.allocationCaches.empty());
}

struct ImageAllocationCacheCreateFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        this->execEnv->unifiedMemoryReuseCleaner.reset(new NEO::MockUnifiedMemoryReuseCleaner(false));
        this->reuseCleaner = static_cast<NEO::MockUnifiedMemoryReuseCleaner *>(this->execEnv->unifiedMemoryReuseCleaner.get());
        this->neoDevice->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
    }

    void tearDown() {
        this->execEnv->unifiedMemoryReuseCleaner.reset();
        DeviceFixture::tearDown();
    }

    NEO::MockUnifiedMemoryReuseCleaner *reuseCleaner = nullptr;
    DebugManagerStateRestore restorer;
};

using ImageAllocationCacheCreateTest = Test<ImageAllocationCacheCreateFixture>;

TEST_F(ImageAllocationCacheCreateTest, givenCacheDisabledByDebugFlagWhenCreatingThenNullptrIsReturned) {
    NEO::debugManager.flags.ExperimentalEnableImageAllocationCache.set(0);

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));
}

TEST_F(ImageAllocationCacheCreateTest, givenCacheEnabledByDebugFlagWhenCreatingThenCacheIsRegisteredInReuseCleanerUntilDestroyed) {
    NEO::debugManager.flags.ExperimentalEnableImageAllocationCache.set(1);

    auto cache = ImageAllocationCache::create(*this->neoDevice);
    ASSERT_NE(nullptr, cache);
    ASSERT_EQ(1u, this->reuseCleaner->allocationCaches.size());
    EXPECT_EQ(cache.get(), this->reuseCleaner->allocationCaches[0]);

    cache.reset();
    EXPECT_TRUE(this->reuseCleaner->allocationCaches.empty());
}

TEST_F(ImageAllocationCacheCreateTest, givenCacheEnabledByDebugFlagAndNoReuseBudgetWhenCreatingThenNullptrIsReturned) {
    NEO::debugManager.flags.ExperimentalEnableImageAllocationCache.set(1);
    this->neoDevice->usmReuseInfo.init(0u, NEO::UsmReuseInfo::notLimited);

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));
}

TEST_F(ImageAllocationCacheCreateTest, givenDefaultSettingsAndProductSupportingDeviceUsmReuseWhenCreatingThenCacheIsCreatedAndRegisteredInReuseCleaner) {
    NEO::RAIIProductHelperFactory<NEO::MockProductHelper> raii(*this->execEnv->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]);
    raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;

    auto cache = ImageAllocationCache::create(*this->neoDevice);
    ASSERT_NE(nullptr, cache);
    ASSERT_EQ(1u, this->reuseCleaner->allocationCaches.size());
    EXPECT_EQ(cache.get(), this->reuseCleaner->allocationCaches[0]);
}

TEST_F(ImageAllocationCacheCreateTest, givenDefaultSettingsAndProductNotSupportingDeviceUsmReuseWhenCreatingThenNullptrIsReturned) {
    NEO::RAIIProductHelperFactory<NEO::MockProductHelper> raii(*this->execEnv->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]);
    raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = false;

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));
    EXPECT_TRUE(this->reuseCleaner->allocationCaches.empty());
}

TEST_F(ImageAllocationCacheCreateTest, givenDefaultSettingsAndDebuggerWhenCreatingThenNullptrIsReturned) {
    NEO::RAIIProductHelperFactory<NEO::MockProductHelper> raii(*this->execEnv->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]);
    raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
    this->neoDevice->getRootDeviceEnvironmentRef().debugger.reset(new NEO::MockDebugger);
    this->neoDevice->setDebugger(this->neoDevice->getRootDeviceEnvironmentRef().debugger.get());

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));

    this->neoDevice->setDebugger(nullptr);
    this->neoDevice->getRootDeviceEnvironmentRef().debugger.reset(nullptr);
}

TEST_F(ImageAllocationCacheCreateTest, givenDefaultSettingsAndNonHardwareCsrWhenCreatingThenNullptrIsReturned) {
    NEO::RAIIProductHelperFactory<NEO::MockProductHelper> raii(*this->execEnv->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]);
    raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
    NEO::debugManager.flags.SetCommandStreamReceiver.set(static_cast<int32_t>(NEO::CommandStreamReceiverType::tbx));

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));
}

TEST_F(ImageAllocationCacheCreateTest, givenDefaultSettingsAndNoReuseCleanerWhenCreatingThenNullptrIsReturned) {
    NEO::RAIIProductHelperFactory<NEO::MockProductHelper> raii(*this->execEnv->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]);
    raii.mockProductHelper->isDeviceUsmAllocationReuseSupportedResult = true;
    this->execEnv->unifiedMemoryReuseCleaner.reset();

    EXPECT_EQ(nullptr, ImageAllocationCache::create(*this->neoDevice));
}

TEST_F(ImageAllocationCacheCreateTest, givenCacheEnabledByDebugFlagAndNoReuseCleanerWhenCreatingThenCacheIsCreatedWithoutRegistration) {
    NEO::debugManager.flags.ExperimentalEnableImageAllocationCache.set(1);
    this->execEnv->unifiedMemoryReuseCleaner.reset();

    auto cache = ImageAllocationCache::create(*this->neoDevice);
    EXPECT_NE(nullptr, cache);
    EXPECT_EQ(nullptr, this->execEnv->unifiedMemoryReuseCleaner.get());
}

TEST_F(ImageAllocationCacheCreateTest, givenDeviceLimitingAllocationsReuseWhenInsertingThenItIsRejected) {
    MockImageAllocationCache cache(*this->neoDevice, nullptr);
    auto allocation = this->neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(NEO::MockAllocationProperties{this->neoDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    NEO::ImageInfo imgInfo = {};
    EXPECT_TRUE(cache.insert(createImageAllocationCacheKey(16u), {}, allocation));
    EXPECT_EQ(allocation, cache.get(createImageAllocationCacheKey(16u), imgInfo));

    this->neoDevice->usmReuseInfo.init(MemoryConstants::gigaByte, 0u);
    this->neoDevice->getMemoryManager()->usmReuseInfo.init(MemoryConstants::gigaByte, 0u);
    ASSERT_TRUE(this->neoDevice->shouldLimitAllocationsReuse());

    EXPECT_FALSE(cache.insert(createImageAllocationCacheKey(16u), {}, allocation));
    EXPECT_TRUE(cache.isEmpty());

    this->neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
}

struct ImageAllocationReuseFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        this->neoDevice->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->driverHandle->imageAllocationCache = std::make_unique<ImageAllocationCache>(*this->neoDevice, nullptr);
        this->cache = this->driverHandle->imageAllocationCache.get();
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    static ze_image_desc_t createImageDesc(uint32_t width) {
        ze_image_desc_t desc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
        desc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
        desc.type = ZE_IMAGE_TYPE_2D;
        desc.format = {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                       ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                       ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
        desc.width = width;
        desc.height = 32u;
        desc.depth = 1u;
        return desc;
    }

    ImageAllocationCache *cache = nullptr;
};

using ImageAllocationReuseTest = Test<ImageAllocationReuseFixture>;

HWTEST2_F(ImageAllocationReuseTest, givenDestroyedImageWhenCreatingImageWithSameDescThenGraphicsAllocationIsReused, ImageSupport) {
    const auto desc = createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    auto allocation = image->getAllocation();
    image->destroy();
    EXPECT_FALSE(this->cache->isEmpty());

    L0::Image *secondImage = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &secondImage));
    EXPECT_EQ(allocation, secondImage->getAllocation());
    EXPECT_TRUE(this->cache->isEmpty());
    secondImage->destroy();
}

HWTEST2_F(ImageAllocationReuseTest, givenDestroyedImageWhenCreatingImageWithDifferentDescThenGraphicsAllocationIsNotReused, ImageSupport) {
    const auto desc = createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    auto allocation = image->getAllocation();
    image->destroy();

    const auto otherDesc = createImageDesc(64u);
    L0::Image *secondImage = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &otherDesc, &secondImage));
    EXPECT_NE(allocation, secondImage->getAllocation());
    EXPECT_FALSE(this->cache->isEmpty());
    secondImage->destroy();
}

HWTEST2_F(ImageAllocationReuseTest, givenImageViewWhenDestroyingViewThenNothingIsCached, ImageSupport) {
    const auto desc = createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));

    ze_image_handle_t viewHandle = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, image->createView(this->device, &desc, &viewHandle));
    Image::fromHandle(viewHandle)->destroy();
    EXPECT_TRUE(this->cache->isEmpty());

    image->destroy();
    EXPECT_FALSE(this->cache->isEmpty());
}

HWTEST2_F(ImageAllocationReuseTest, givenImageFromBufferWhenDestroyingImageThenNothingIsCached, ImageSupport) {
    const uint32_t width = 256u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->context->allocDeviceMem(this->device->toHandle(), &deviceDesc, width * sizeof(uint32_t), 0, &ptr));

    ze_image_pitched_exp_desc_t pitchedDesc = {ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC};
    pitchedDesc.ptr = ptr;
    auto desc = createImageDesc(width);
    desc.pNext = &pitchedDesc;
    desc.type = ZE_IMAGE_TYPE_BUFFER;
    desc.height = 1u;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32;

    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    image->destroy();
    EXPECT_TRUE(this->cache->isEmpty());

    EXPECT_EQ(ZE_RESULT_SUCCESS, this->context->freeMem(ptr));
}

HWTEST2_F(ImageAllocationReuseTest, givenExhaustedReuseBudgetWhenDestroyingImageThenAllocationIsFreed, ImageSupport) {
    this->neoDevice->usmReuseInfo.init(1u, NEO::UsmReuseInfo::notLimited);
    const auto desc = createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    image->destroy();
    EXPECT_TRUE(this->cache->isEmpty());
}

HWTEST2_F(ImageAllocationReuseTest, givenBindlessImageWhenGraphicsAllocationIsReusedThenPreviousBindlessSlotIsReleasedAndNewOneIsAllocated, ImageSupport) {
    auto bindlessHelper = new MockBindlesHeapsHelper(this->neoDevice, this->neoDevice->getNumGenericSubDevices() > 1);
    this->neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[this->neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHelper);

    ze_image_bindless_exp_desc_t bindlessExtDesc = {ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC};
    bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;
    auto desc = createImageDesc(32u);
    desc.pNext = &bindlessExtDesc;

    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    auto allocation = image->getAllocation();
    const auto firstBindlessOffset = allocation->getBindlessOffset();
    EXPECT_NE(std::numeric_limits<uint64_t>::max(), firstBindlessOffset);
    image->destroy();

    auto releasedSlotCount = [&]() {
        return bindlessHelper->surfaceStateInHeapVectorReuse[bindlessHelper->releasePoolIndex][0].size() +
               bindlessHelper->surfaceStateInHeapVectorReuse[bindlessHelper->releasePoolIndex][1].size();
    };
    const auto releasedSlotCountBefore = releasedSlotCount();

    L0::Image *secondImage = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &secondImage));
    EXPECT_EQ(allocation, secondImage->getAllocation());
    EXPECT_EQ(releasedSlotCountBefore + 1u, releasedSlotCount());
    EXPECT_NE(std::numeric_limits<uint64_t>::max(), allocation->getBindlessOffset());
    EXPECT_NE(firstBindlessOffset, allocation->getBindlessOffset());
    EXPECT_NE(nullptr, secondImage->getBindlessSlot());
    secondImage->destroy();
}

struct ImageAllocationReuseWithMockMemoryManagerFixture : public DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager> {
    void setUp() {
        DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager>::setUp();
        this->neoDevice->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->memoryManager->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->driverHandle->imageAllocationCache = std::make_unique<ImageAllocationCache>(*this->neoDevice, nullptr);
        this->cache = this->driverHandle->imageAllocationCache.get();
    }

    void tearDown() {
        DeviceFixtureWithCustomMemoryManager<NEO::MockMemoryManager>::tearDown();
    }

    ImageAllocationCache *cache = nullptr;
};

using ImageAllocationReuseWithMockMemoryManagerTest = Test<ImageAllocationReuseWithMockMemoryManagerFixture>;

HWTEST2_F(ImageAllocationReuseWithMockMemoryManagerTest, givenCachedAllocationWhenAllocationForNewImageFailsOnceThenCacheIsTrimmedAndAllocationIsRetried, ImageSupport) {
    const auto desc = ImageAllocationReuseFixture::createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    image->destroy();
    ASSERT_FALSE(this->cache->isEmpty());

    this->memoryManager->isMockHostMemoryManager = true;
    this->memoryManager->forceFailureInPrimaryAllocation = true;
    this->memoryManager->singleFailureInPrimaryAllocation = true;
    const uint32_t allocationCallsBefore = this->memoryManager->allocateGraphicsMemoryWithPropertiesCount;

    const auto otherDesc = ImageAllocationReuseFixture::createImageDesc(64u);
    L0::Image *secondImage = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &otherDesc, &secondImage));
    EXPECT_NE(nullptr, secondImage->getAllocation());
    EXPECT_TRUE(this->cache->isEmpty());
    EXPECT_EQ(allocationCallsBefore + 2u, this->memoryManager->allocateGraphicsMemoryWithPropertiesCount);
    secondImage->destroy();
}

struct MemoryManagerFailingWhileImagesAreCached : public NEO::MockMemoryManager {
    using NEO::MockMemoryManager::allocateGraphicsMemoryWithProperties;
    using NEO::MockMemoryManager::MockMemoryManager;

    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const NEO::AllocationProperties &properties, const void *ptr) override {
        if (this->imageAllocationCache && !this->imageAllocationCache->isEmpty()) {
            this->failedAllocationsCount++;
            return nullptr;
        }
        return NEO::MockMemoryManager::allocateGraphicsMemoryWithProperties(properties, ptr);
    }

    ImageAllocationCache *imageAllocationCache = nullptr;
    uint32_t failedAllocationsCount = 0u;
};

struct ImageAllocationCacheDeviceMemFixture : public DeviceFixtureWithCustomMemoryManager<MemoryManagerFailingWhileImagesAreCached> {
    void setUp() {
        DeviceFixtureWithCustomMemoryManager<MemoryManagerFailingWhileImagesAreCached>::setUp();
        this->neoDevice->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->memoryManager->usmReuseInfo.init(MemoryConstants::gigaByte, NEO::UsmReuseInfo::notLimited);
        this->driverHandle->imageAllocationCache = std::make_unique<ImageAllocationCache>(*this->neoDevice, nullptr);
        this->cache = this->driverHandle->imageAllocationCache.get();
        this->memoryManager->imageAllocationCache = this->cache;
    }

    void tearDown() {
        this->memoryManager->imageAllocationCache = nullptr;
        DeviceFixtureWithCustomMemoryManager<MemoryManagerFailingWhileImagesAreCached>::tearDown();
    }

    ImageAllocationCache *cache = nullptr;
};

using ImageAllocationCacheDeviceMemTest = Test<ImageAllocationCacheDeviceMemFixture>;

HWTEST2_F(ImageAllocationCacheDeviceMemTest, givenCachedImageAllocationWhenDeviceAllocationFailsThenImageCacheIsTrimmedAndAllocationIsRetried, ImageSupport) {
    const auto desc = ImageAllocationReuseFixture::createImageDesc(32u);
    L0::Image *image = nullptr;
    ASSERT_EQ(ZE_RESULT_SUCCESS, Image::create(this->device, &desc, &image));
    image->destroy();
    ASSERT_FALSE(this->cache->isEmpty());

    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    EXPECT_EQ(ZE_RESULT_SUCCESS, this->context->allocDeviceMem(this->device->toHandle(), &deviceDesc, MemoryConstants::pageSize, 0u, &ptr));
    EXPECT_NE(nullptr, ptr);
    EXPECT_TRUE(this->cache->isEmpty());
    EXPECT_LT(0u, this->memoryManager->failedAllocationsCount);

    EXPECT_EQ(ZE_RESULT_SUCCESS, this->context->freeMem(ptr));
}

} // namespace ult
} // namespace L0
