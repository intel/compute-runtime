/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include "test_traits_common.h"

using namespace NEO;

template <bool useLocalMemory = true>
struct CompressionXeHPAndLater : public AUBFixture,
                                 public ::testing::Test,
                                 public ::testing::WithParamInterface<uint32_t /*EngineType*/> {
    void SetUp() override {
        REQUIRE_64BIT_OR_SKIP();

        auto &ftrTable = defaultHwInfo->featureTable;
        if ((!ftrTable.flags.ftrFlatPhysCCS) ||
            (!ftrTable.flags.ftrLocalMemory && useLocalMemory)) {
            GTEST_SKIP();
        }

        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.RenderCompressedImagesEnabled.set(true);
        debugManager.flags.EnableLocalMemory.set(useLocalMemory);
        debugManager.flags.NodeOrdinal.set(GetParam());

        AUBFixture::setUp(defaultHwInfo.get());

        auto &gfxCoreHelper = device->getGfxCoreHelper();

        auto expectedEngine = static_cast<aub_stream::EngineType>(GetParam());
        bool engineSupported = false;
        for (auto &engine : gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment())) {
            if (engine.first == expectedEngine) {
                engineSupported = true;
                break;
            }
        }

        if (!engineSupported) {
            GTEST_SKIP();
        }

        context->contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    }
    void TearDown() override {
        AUBFixture::tearDown();
    }
    std::unique_ptr<DebugManagerStateRestore> debugRestorer;

    cl_int retVal = CL_SUCCESS;

    template <typename FamilyType>
    void givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect();
    template <typename FamilyType>
    void givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect();
    template <typename FamilyType>
    void givenCompressedImageWhenReadingThenResultsAreCorrect();
};

template <bool testLocalMemory>
template <typename FamilyType>
void CompressionXeHPAndLater<testLocalMemory>::givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect() {
    const size_t bufferSize = 2048;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto compressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, nullptr, retVal));
    auto compressedAllocation = compressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    memset(compressedAllocation->getUnderlyingBuffer(), 0, bufferSize);
    EXPECT_NE(nullptr, compressedAllocation->getDefaultGmm()->gmmResourceInfo->peekHandle());
    EXPECT_TRUE(compressedAllocation->getDefaultGmm()->isCompressionEnabled());
    if (testLocalMemory) {
        EXPECT_EQ(MemoryPool::localMemory, compressedAllocation->getMemoryPool());
    } else {
        EXPECT_EQ(MemoryPool::system4KBPages, compressedAllocation->getMemoryPool());
    }

    auto notCompressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    auto nonCompressedAllocation = notCompressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    nonCompressedAllocation->setAllocationType(AllocationType::buffer);
    if (nonCompressedAllocation->getDefaultGmm()) {
        nonCompressedAllocation->getDefaultGmm()->setCompressionEnabled(false);
    }
    memset(nonCompressedAllocation->getUnderlyingBuffer(), 0, bufferSize);

    pCmdQ->enqueueWriteBuffer(compressedBuffer.get(), CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
    pCmdQ->enqueueCopyBuffer(compressedBuffer.get(), notCompressedBuffer.get(), 0, 0, bufferSize, 0, nullptr, nullptr);
    pCmdQ->finish(false);

    expectNotEqualMemory<FamilyType>(AUBFixture::getGpuPointer(compressedAllocation),
                                     writePattern, bufferSize);

    expectMemory<FamilyType>(AUBFixture::getGpuPointer(nonCompressedAllocation),
                             writePattern, bufferSize);
}

template <bool testLocalMemory>
template <typename FamilyType>
void CompressionXeHPAndLater<testLocalMemory>::givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect() {
    const size_t imageWidth = 16;
    const size_t imageHeight = 16;

    const size_t bufferSize = 64 * MemoryConstants::kiloByte;
    uint8_t writePattern[bufferSize];
    std::fill(writePattern, writePattern + sizeof(writePattern), 1);

    device->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);

    auto compressedBuffer = std::unique_ptr<Buffer>(Buffer::create(context, CL_MEM_COPY_HOST_PTR | CL_MEM_COMPRESSED_HINT_INTEL, bufferSize, writePattern, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    // now create image2DFromBuffer

    cl_image_desc imageDescriptor = {};
    imageDescriptor.mem_object = compressedBuffer.get();
    imageDescriptor.image_height = imageWidth;
    imageDescriptor.image_width = imageHeight;
    imageDescriptor.image_type = CL_MEM_OBJECT_IMAGE2D;
    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT32;
    imageFormat.image_channel_order = CL_RGBA;

    auto clCompressedImage = clCreateImage(context, CL_MEM_READ_WRITE, &imageFormat, &imageDescriptor, nullptr, &retVal);
    auto compressedImage = castToObject<Image>(clCompressedImage);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const size_t perChannelDataSize = sizeof(cl_uint);
    const size_t numChannels = 4;
    const auto imageSize = imageWidth * imageHeight * perChannelDataSize * numChannels;
    cl_uint destMemory[imageSize / sizeof(cl_uint)] = {0};
    const size_t origin[] = {0, 0, 0};
    const size_t region[] = {imageWidth, imageHeight, 1};

    retVal = pCmdQ->enqueueReadImage(
        compressedImage,
        CL_FALSE,
        origin,
        region,
        0,
        0,
        destMemory,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(destMemory, writePattern, imageSize);

    // make sure our objects are in in fact compressed
    auto graphicsAllocation = compressedBuffer->getGraphicsAllocation(device->getRootDeviceIndex());
    EXPECT_NE(nullptr, graphicsAllocation->getDefaultGmm());
    EXPECT_TRUE(graphicsAllocation->getDefaultGmm()->isCompressionEnabled());
    EXPECT_TRUE(compressedImage->getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled());

    expectNotEqualMemory<FamilyType>(addrToPtr(ptrOffset(graphicsAllocation->getGpuAddress(), compressedBuffer->getOffset())), writePattern, bufferSize);

    clReleaseMemObject(clCompressedImage);
}

template <bool testLocalMemory>
template <typename FamilyType>
void CompressionXeHPAndLater<testLocalMemory>::givenCompressedImageWhenReadingThenResultsAreCorrect() {
    const size_t imageWidth = 8;
    const size_t imageHeight = 4;
    const size_t perChannelDataSize = sizeof(cl_float);
    const size_t numChannels = 4;
    const auto imageSize = imageWidth * imageHeight * perChannelDataSize * numChannels;
    const auto rowSize = imageSize / imageHeight;
    cl_float srcMemory[imageSize / sizeof(cl_float)] = {0};

    const cl_float row[rowSize] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                   1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    cl_float *pixel = srcMemory;
    for (uint32_t height = 0; height < imageHeight; height++) {
        memcpy(pixel, row, rowSize);
        pixel += imageWidth;
    }

    cl_float destMemory[imageSize / sizeof(cl_float)] = {0};

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    imageFormat.image_channel_data_type = CL_FLOAT;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = imageWidth;
    imageDesc.image_height = imageHeight;
    imageDesc.image_depth = 1;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    auto allocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, imageSize}, destMemory);
    csr->makeResidentHostPtrAllocation(allocation);
    csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), TEMPORARY_ALLOCATION);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Image> srcImage(Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        srcMemory,
        retVal));
    ASSERT_NE(nullptr, srcImage);

    cl_bool blockingRead = CL_FALSE;
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    const size_t origin[] = {0, 0, 0};
    const size_t region[] = {imageWidth, imageHeight, 1};

    retVal = pCmdQ->enqueueReadImage(
        srcImage.get(),
        blockingRead,
        origin,
        region,
        0,
        0,
        destMemory,
        nullptr,
        numEventsInWaitList,
        eventWaitList,
        event);
    EXPECT_EQ(CL_SUCCESS, retVal);

    allocation = pCmdQ->getGpgpuCommandStreamReceiver().getTemporaryAllocations().peekHead();
    while (allocation && allocation->getUnderlyingBuffer() != destMemory) {
        allocation = allocation->next;
    }
    auto pDestGpuAddress = reinterpret_cast<void *>(allocation->getGpuAddress());

    pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    expectMemory<FamilyType>(pDestGpuAddress, srcMemory, imageSize);
    expectNotEqualMemory<FamilyType>(AUBFixture::getGpuPointer(srcImage->getGraphicsAllocation(rootDeviceIndex)), srcMemory, imageSize);
}

struct CompressionLocalAubsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::localMemCompressionAubsSupported;
        }
        return false;
    }
};

struct CompressionSystemAubsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::systemMemCompressionAubsSupported;
        }
        return false;
    }
};

using CompressionLocalXeHPAndLater = CompressionXeHPAndLater<true>;
HWTEST2_P(CompressionLocalXeHPAndLater, givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect, CompressionLocalAubsSupportedMatcher) {
    givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect<FamilyType>();
}
HWTEST2_P(CompressionLocalXeHPAndLater, givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect, CompressionLocalAubsSupportedMatcher) {
    givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect<FamilyType>();
}
HWTEST2_P(CompressionLocalXeHPAndLater, givenCompressedImageWhenReadingThenResultsAreCorrect, CompressionLocalAubsSupportedMatcher) {
    givenCompressedImageWhenReadingThenResultsAreCorrect<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(,
                         CompressionLocalXeHPAndLater,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));

using CompressionSystemXeHPAndLater = CompressionXeHPAndLater<false>;
HWTEST2_P(CompressionSystemXeHPAndLater, GENERATEONLY_givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect, CompressionSystemAubsSupportedMatcher) {
    givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect<FamilyType>();
}
HWTEST2_P(CompressionSystemXeHPAndLater, GENERATEONLY_givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect, CompressionSystemAubsSupportedMatcher) {
    givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect<FamilyType>();
}
HWTEST2_P(CompressionSystemXeHPAndLater, givenCompressedImageWhenReadingThenResultsAreCorrect, CompressionSystemAubsSupportedMatcher) {
    givenCompressedImageWhenReadingThenResultsAreCorrect<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(,
                         CompressionSystemXeHPAndLater,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));
