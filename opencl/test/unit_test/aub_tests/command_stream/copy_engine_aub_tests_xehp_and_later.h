/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

template <uint32_t numTiles, bool useLocalMemory = true>
struct CopyEngineXeHPAndLater : public MulticontextOclAubFixture, public ::testing::Test {
    using MulticontextOclAubFixture::expectMemory;

    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }

        MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->template getHelper<ProductHelper>();
        if (!productHelper.obtainBlitterPreference(*defaultHwInfo.get())) {
            GTEST_SKIP();
        }
        bcsEngineType = productHelper.getDefaultCopyEngine();

        if (useLocalMemory) {
            if (!defaultHwInfo->featureTable.flags.ftrLocalMemory) {
                GTEST_SKIP();
            }
            debugManager.flags.EnableLocalMemory.set(true);
        }

        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.RenderCompressedImagesEnabled.set(true);
        debugManager.flags.EnableFreeMemory.set(false);

        MulticontextOclAubFixture::setUp(numTiles, EnabledCommandStreamers::single, true);

        defaultCommandQueue = commandQueues[0][0].get();
        bcsCsr = tileDevices[0]->getNearestGenericSubDevice(0)->getEngine(bcsEngineType, EngineUsage::regular).commandStreamReceiver;

        compressiblePattern = std::make_unique<uint8_t[]>(bufferSize);
        std::fill(compressiblePattern.get(), ptrOffset(compressiblePattern.get(), bufferSize), 0xC6);

        writePattern = std::make_unique<uint8_t[]>(bufferSize);
        dstHostPtr = std::make_unique<uint8_t[]>(bufferSize);
        for (size_t i = 0; i < bufferSize; i++) {
            writePattern.get()[i] = static_cast<uint8_t>(i);
            dstHostPtr.get()[i] = 255 - writePattern[i];
        }

        EXPECT_NE(writePattern.get()[0], writePattern.get()[offset]);
    }

    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
    }

    virtual bool compressionSupported() const {
        auto &ftrTable = rootDevice->getHardwareInfo().featureTable;
        return (ftrTable.flags.ftrLocalMemory && ftrTable.flags.ftrFlatPhysCCS);
    }

    ReleaseableObjectPtr<Buffer> createBuffer(bool compressed, bool inLocalMemory, void *srcHostPtr) {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        if (!compressed) {
            flags |= CL_MEM_UNCOMPRESSED_HINT_INTEL;
        } else {
            flags |= CL_MEM_COMPRESSED_HINT_INTEL;
        }

        if (!inLocalMemory && !compressed) {
            flags |= CL_MEM_FORCE_HOST_MEMORY_INTEL;
        }
        if (srcHostPtr) {
            flags |= CL_MEM_COPY_HOST_PTR;
        }

        auto buffer = clUniquePtr(Buffer::create(context.get(), flags, bufferSize, srcHostPtr, retVal));
        auto graphicsAllocation = buffer->getGraphicsAllocation(this->rootDeviceIndex);

        EXPECT_EQ(CL_SUCCESS, retVal);
        if (compressed) {
            EXPECT_TRUE(graphicsAllocation->isCompressionEnabled());
        }
        EXPECT_EQ(!inLocalMemory, MemoryPoolHelper::isSystemMemoryPool(graphicsAllocation->getMemoryPool()));

        return buffer;
    }

    void *getGpuAddress(Buffer &buffer) {
        return addrToPtr(getGpuVA(buffer));
    }

    uint64_t getGpuVA(Buffer &buffer) {
        return ptrOffset(buffer.getGraphicsAllocation(this->rootDeviceIndex)->getGpuAddress(), buffer.getOffset());
    }

    void executeBlitCommand(const BlitProperties &blitProperties, bool blocking) {
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);
        bcsCsr->flushBcsTask(blitPropertiesContainer, blocking, rootDevice->getDevice());
    }

    template <typename FamilyType>
    void givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl();
    template <typename FamilyType>
    void givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl();
    template <typename FamilyType>
    void givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl();
    template <typename FamilyType>
    void GivenReadOnlyMultiStorageWhenAllocatingBufferThenAllocationIsCopiedWithBlitterToEveryTileImpl(); // NOLINT(readability-identifier-naming)
    template <typename FamilyType>
    void givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();

    DebugManagerStateRestore restore;
    CommandQueue *defaultCommandQueue = nullptr;
    CommandStreamReceiver *bcsCsr = nullptr;
    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    static constexpr size_t bufferSize = MemoryConstants::pageSize64k + BlitterConstants::maxBlitWidth + 3;
    static constexpr size_t offset = (bufferSize / 4) - 3;
    aub_stream::EngineType bcsEngineType = aub_stream::EngineType::ENGINE_BCS;

    std::unique_ptr<uint8_t[]> compressiblePattern;
    std::unique_ptr<uint8_t[]> writePattern;
    std::unique_ptr<uint8_t[]> dstHostPtr;
    cl_int retVal = CL_SUCCESS;
};

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl() {
    if (!compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcNotCompressedBuffer = createBuffer(false, testLocalMemory, compressiblePattern.get());
    auto dstNotCompressedBuffer = createBuffer(false, testLocalMemory, nullptr);
    auto dstCompressedBuffer = createBuffer(true, testLocalMemory, nullptr);
    auto dstResolvedBuffer = createBuffer(false, testLocalMemory, nullptr);

    // Buffer to Buffer - uncompressed HBM -> compressed HBM
    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcNotCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        {dstCompressedBuffer->getOffset(), 0, 0}, {srcNotCompressedBuffer->getOffset(), 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());
    executeBlitCommand(blitProperties, true);
    // Buffer to Buffer - uncompressed HBM -> uncompressed HBM
    blitProperties = BlitProperties::constructPropertiesForCopy(
        dstNotCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcNotCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        {dstNotCompressedBuffer->getOffset(), 0, 0}, {srcNotCompressedBuffer->getOffset(), 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());
    executeBlitCommand(blitProperties, true);
    // Buffer to Buffer - compressed HBM -> uncompressed HBM
    blitProperties = BlitProperties::constructPropertiesForCopy(
        dstResolvedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        dstCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        {dstResolvedBuffer->getOffset(), 0, 0}, {dstCompressedBuffer->getOffset(), 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());
    executeBlitCommand(blitProperties, true);

    blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *bcsCsr,
                                                                     dstCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), nullptr,
                                                                     dstHostPtr.get(),
                                                                     getGpuVA(*dstCompressedBuffer), 0,
                                                                     0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    executeBlitCommand(blitProperties, true);
    expectMemoryNotEqual<FamilyType>(getGpuAddress(*dstCompressedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(getGpuAddress(*dstResolvedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(getGpuAddress(*dstNotCompressedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(getGpuAddress(*srcNotCompressedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!compressionSupported()) {
        GTEST_SKIP();
    }

    auto dstCompressedBuffer = createBuffer(true, testLocalMemory, nullptr);

    // HostPtr to Buffer - System -> compressed HBM
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, dstCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), nullptr,
                                                                          compressiblePattern.get(),
                                                                          getGpuVA(*dstCompressedBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, true);

    blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *bcsCsr,
                                                                     dstCompressedBuffer->getGraphicsAllocation(rootDeviceIndex), nullptr,
                                                                     dstHostPtr.get(),
                                                                     getGpuVA(*dstCompressedBuffer), 0,
                                                                     0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    executeBlitCommand(blitProperties, true);

    expectMemoryNotEqual<FamilyType>(getGpuAddress(*dstCompressedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcCompressedBuffer = createBuffer(true, testLocalMemory, nullptr);
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, srcCompressedBuffer->getGraphicsAllocation(rootDeviceIndex),
                                                                          nullptr, compressiblePattern.get(),
                                                                          getGpuVA(*srcCompressedBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    executeBlitCommand(blitProperties, true);

    // Buffer to HostPtr - compressed HBM -> System
    blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                     *bcsCsr, srcCompressedBuffer->getGraphicsAllocation(rootDeviceIndex),
                                                                     nullptr, dstHostPtr.get(),
                                                                     getGpuVA(*srcCompressedBuffer), 0,
                                                                     0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    expectMemoryNotEqual<FamilyType>(getGpuAddress(*srcCompressedBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), compressiblePattern.get(), bufferSize, 0, 0);

    srcCompressedBuffer.reset();
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto srcNotCompressedLocalBuffer = createBuffer(false, testLocalMemory, compressiblePattern.get());

    // Buffer to HostPtr - uncompressed HBM -> System
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          *bcsCsr, srcNotCompressedLocalBuffer->getGraphicsAllocation(rootDeviceIndex),
                                                                          nullptr, dstHostPtr.get(),
                                                                          getGpuVA(*srcNotCompressedLocalBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    expectMemory<FamilyType>(getGpuAddress(*srcNotCompressedLocalBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto buffer = createBuffer(false, testLocalMemory, nullptr);

    // HostPtr to Buffer - System -> uncompressed HBM
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, buffer->getGraphicsAllocation(rootDeviceIndex), nullptr, compressiblePattern.get(),
                                                                          getGpuVA(*buffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, true);

    expectMemory<FamilyType>(getGpuAddress(*buffer), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto buffer = createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, buffer->getGraphicsAllocation(rootDeviceIndex), nullptr, writePattern.get(),
                                                                          getGpuVA(*buffer), 0,
                                                                          0, {offset, 0, 0}, {bufferSize - offset, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, true);

    expectMemoryNotEqual<FamilyType>(getGpuAddress(*buffer), writePattern.get(), bufferSize, 0, 0);
    expectMemoryNotEqual<FamilyType>(ptrOffset(getGpuAddress(*buffer), offset - 1), writePattern.get(), bufferSize - offset, 0, 0);
    expectMemory<FamilyType>(ptrOffset(getGpuAddress(*buffer), offset), writePattern.get(), bufferSize - offset, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto srcBuffer = createBuffer(false, testLocalMemory, writePattern.get());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          *bcsCsr, srcBuffer->getGraphicsAllocation(rootDeviceIndex), nullptr, dstHostPtr.get(),
                                                                          getGpuVA(*srcBuffer), 0,
                                                                          0, {offset, 0, 0}, {bufferSize - offset, 1, 1}, 0, 0, 0, 0);

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    expectMemoryNotEqual<FamilyType>(dstHostPtr.get(), writePattern.get(), offset, 0, 0);
    expectMemoryNotEqual<FamilyType>(ptrOffset(dstHostPtr.get(), 1), ptrOffset(writePattern.get(), offset), bufferSize - offset - 1, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), ptrOffset(writePattern.get(), offset), bufferSize - offset, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl() {
    size_t copiedSize = bufferSize - (2 * offset);

    auto srcBuffer = createBuffer(false, testLocalMemory, writePattern.get());
    auto dstBuffer = createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        {offset + dstBuffer->getOffset(), 0, 0}, {srcBuffer->getOffset(), 0, 0}, {copiedSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());

    executeBlitCommand(blitProperties, true);

    expectMemoryNotEqual<FamilyType>(getGpuAddress(*dstBuffer), writePattern.get(), bufferSize, 0, 0);
    expectMemoryNotEqual<FamilyType>(getGpuAddress(*dstBuffer), writePattern.get(), copiedSize + 1, 0, 0);
    expectMemoryNotEqual<FamilyType>(ptrOffset(getGpuAddress(*dstBuffer), offset - 1), writePattern.get(), copiedSize, 0, 0);
    expectMemory<FamilyType>(ptrOffset(getGpuAddress(*dstBuffer), offset), writePattern.get(), copiedSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcBuffer = createBuffer(true, testLocalMemory, nullptr);
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, srcBuffer->getGraphicsAllocation(rootDeviceIndex),
                                                                          nullptr, compressiblePattern.get(),
                                                                          getGpuVA(*srcBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    executeBlitCommand(blitProperties, false);
    auto dstBuffer = createBuffer(true, testLocalMemory, nullptr);

    // Buffer to Buffer - compressed HBM -> compressed HBM
    blitProperties = BlitProperties::constructPropertiesForCopy(
        dstBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());

    executeBlitCommand(blitProperties, true);

    blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *bcsCsr,
                                                                     dstBuffer->getGraphicsAllocation(rootDeviceIndex), nullptr,
                                                                     dstHostPtr.get(),
                                                                     getGpuVA(*dstBuffer), 0,
                                                                     0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    executeBlitCommand(blitProperties, true);

    expectMemoryNotEqual<FamilyType>(getGpuAddress(*dstBuffer), compressiblePattern.get(), bufferSize, 0, 0);
    expectMemory<FamilyType>(dstHostPtr.get(), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl() {
    if ((this->context->getDevice(0u)->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.sharedSystemMemCapabilities > 0) || !compressionSupported()) {
        // no support for scenarios where stateless is mixed with blitter compression
        GTEST_SKIP();
    }

    auto buffer = createBuffer(true, testLocalMemory, compressiblePattern.get());

    {
        // initialized as compressed
        expectMemoryNotEqual<FamilyType>(getGpuAddress(*buffer), compressiblePattern.get(), bufferSize, 0, 0);
    }

    {
        // resolve
        auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::auxToNonAux,
                                                                                   buffer->getGraphicsAllocation(rootDeviceIndex),
                                                                                   bcsCsr->getClearColorAllocation());

        executeBlitCommand(blitProperties, false);
        bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

        expectMemory<FamilyType>(getGpuAddress(*buffer), compressiblePattern.get(), bufferSize, 0, 0);
    }

    {
        // compress again
        auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::nonAuxToAux,
                                                                                   buffer->getGraphicsAllocation(rootDeviceIndex),
                                                                                   bcsCsr->getClearColorAllocation());

        executeBlitCommand(blitProperties, false);
        bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

        expectMemoryNotEqual<FamilyType>(getGpuAddress(*buffer), compressiblePattern.get(), bufferSize, 0, 0);
    }
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto srcBuffer = createBuffer(false, false, compressiblePattern.get());
    auto dstBuffer = createBuffer(false, false, nullptr);

    // Buffer to Buffer - System -> System
    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0, bcsCsr->getClearColorAllocation());

    executeBlitCommand(blitProperties, true);

    expectMemory<FamilyType>(getGpuAddress(*dstBuffer), compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto srcMemory = std::make_unique<uint8_t[]>(bufferSize);
    auto destMemory = std::make_unique<uint8_t[]>(bufferSize);

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(1);
        destMemory[i] = static_cast<uint8_t>(2);
    }

    auto srcBuffer = createBuffer(false, testLocalMemory, srcMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    size_t hostOrigin[] = {0, 0, 0};
    size_t bufferOrigin[] = {1 * sizeof(uint8_t), 1, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 1};
    size_t hostRowPitch = 2 * sizeof(uint8_t);
    size_t hostSlicePitch = 4 * sizeof(uint8_t);
    size_t bufferRowPitch = 4 * sizeof(uint8_t);
    size_t bufferSlicePitch = 8 * sizeof(uint8_t);

    EXPECT_TRUE(srcBuffer->bufferRectPitchSet(bufferOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch, true));

    size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);

    HostPtrSurface hostPtrSurface(destMemory.get(), hostPtrSize, true);
    bcsCsr->createAllocationForHostSurface(hostPtrSurface, true);
    GraphicsAllocation *allocation = hostPtrSurface.getAllocation();
    auto srcAllocation = srcBuffer->getGraphicsAllocation(rootDeviceIndex);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, // blitDirection
                                                                          *bcsCsr,                                          // commandStreamReceiver
                                                                          srcAllocation,                                    // memObjAllocation
                                                                          allocation,                                       // preallocatedHostAllocation
                                                                          pDestMemory,                                      // hostPtr
                                                                          getGpuVA(*srcBuffer),                             // memObjGpuVa
                                                                          allocation->getGpuAddress(),                      // hostAllocGpuVa
                                                                          hostOrigin,                                       // hostPtrOffset
                                                                          bufferOrigin,                                     // copyOffset
                                                                          region,                                           // copySize
                                                                          hostRowPitch,                                     // hostRowPitch
                                                                          hostSlicePitch,                                   // hostSlicePitch
                                                                          bufferRowPitch,                                   // gpuRowPitch
                                                                          bufferSlicePitch);                                // gpuSlicePitch

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    pSrcMemory = ptrOffset(pSrcMemory, 0);
    int *destGpuAddress = reinterpret_cast<int *>(allocation->getGpuAddress());

    expectMemoryNotEqual<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize + 1, 0, 0);
    expectMemory<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto srcMemory = std::make_unique<uint8_t[]>(bufferSize);
    auto destMemory = std::make_unique<uint8_t[]>(bufferSize);

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(1);
        destMemory[i] = static_cast<uint8_t>(2);
    }

    auto srcBuffer = createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    size_t hostOrigin[] = {0, 0, 0};
    size_t bufferOrigin[] = {1 * sizeof(uint8_t), 1, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 1};
    size_t hostRowPitch = 2 * sizeof(uint8_t);
    size_t hostSlicePitch = 4 * sizeof(uint8_t);
    size_t bufferRowPitch = 4 * sizeof(uint8_t);
    size_t bufferSlicePitch = 8 * sizeof(uint8_t);

    EXPECT_TRUE(srcBuffer->bufferRectPitchSet(bufferOrigin, region, bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch, true));

    size_t hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);

    HostPtrSurface hostPtrSurface(srcMemory.get(), hostPtrSize, true);
    bcsCsr->createAllocationForHostSurface(hostPtrSurface, true);
    GraphicsAllocation *allocation = hostPtrSurface.getAllocation();
    auto srcAllocation = srcBuffer->getGraphicsAllocation(rootDeviceIndex);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer, // blitDirection
                                                                          *bcsCsr,                                          // commandStreamReceiver
                                                                          srcAllocation,                                    // memObjAllocation
                                                                          allocation,                                       // preallocatedHostAllocation
                                                                          pDestMemory,                                      // hostPtr
                                                                          getGpuVA(*srcBuffer),                             // memObjGpuVa
                                                                          allocation->getGpuAddress(),                      // hostAllocGpuVa
                                                                          hostOrigin,                                       // hostPtrOffset
                                                                          bufferOrigin,                                     // copyOffset
                                                                          region,                                           // copySize
                                                                          hostRowPitch,                                     // hostRowPitch
                                                                          hostSlicePitch,                                   // hostSlicePitch
                                                                          bufferRowPitch,                                   // gpuRowPitch
                                                                          bufferSlicePitch);                                // gpuSlicePitch

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    pSrcMemory = ptrOffset(pSrcMemory, 0);
    int *destGpuAddress = reinterpret_cast<int *>(allocation->getGpuAddress());

    expectMemoryNotEqual<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize + 1, 0, 0);
    expectMemory<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto srcMemory = std::make_unique<uint8_t[]>(bufferSize);
    auto destMemory = std::make_unique<uint8_t[]>(bufferSize);

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(1);
        destMemory[i] = static_cast<uint8_t>(2);
    }

    auto srcBuffer = createBuffer(false, testLocalMemory, srcMemory.get());
    auto dstBuffer = createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = reinterpret_cast<uint8_t *>(getGpuAddress((*dstBuffer)));
    auto clearColorAllocation = bcsCsr->getClearColorAllocation();

    size_t srcOrigin[] = {srcBuffer->getOffset(), 0, 0};
    size_t dstOrigin[] = {1 * sizeof(uint8_t) + dstBuffer->getOffset(), 0, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 2};
    size_t srcRowPitch = region[0];
    size_t srcSlicePitch = srcRowPitch * region[1];
    size_t dstRowPitch = region[0];
    size_t dstSlicePitch = dstRowPitch * region[1];
    auto copySize = region[0] * region[1] * region[2];

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        dstOrigin, srcOrigin, region,
        srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, clearColorAllocation);

    executeBlitCommand(blitProperties, false);

    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    pSrcMemory = ptrOffset(pSrcMemory, 0);

    expectMemoryNotEqual<FamilyType>(ptrOffset(pDestMemory, sizeof(uint8_t)), pSrcMemory, copySize + 1, 0, 0);
    expectMemory<FamilyType>(ptrOffset(pDestMemory, sizeof(uint8_t)), pSrcMemory, copySize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    DebugManagerStateRestore restore;
    debugManager.flags.LimitBlitterMaxWidth.set(8);
    debugManager.flags.LimitBlitterMaxHeight.set(8);

    auto srcMemory = std::make_unique<uint8_t[]>(bufferSize);
    auto destMemory = std::make_unique<uint8_t[]>(bufferSize);

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(1);
        destMemory[i] = static_cast<uint8_t>(2);
    }

    auto srcBuffer = createBuffer(false, testLocalMemory, srcMemory.get());
    auto dstBuffer = createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = reinterpret_cast<uint8_t *>(getGpuAddress(*dstBuffer));
    auto clearColorAllocation = bcsCsr->getClearColorAllocation();

    size_t srcOrigin[] = {srcBuffer->getOffset(), 0, 0};
    size_t dstOrigin[] = {1 + dstBuffer->getOffset(), 1, 1};
    size_t region[] = {20, 16, 2};
    size_t srcRowPitch = region[0];
    size_t srcSlicePitch = srcRowPitch * region[1];
    size_t dstRowPitch = region[0];
    size_t dstSlicePitch = dstRowPitch * region[1];
    auto copySize = region[0] * region[1] * region[2];

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        srcBuffer->getGraphicsAllocation(rootDeviceIndex), 0,
        dstOrigin, srcOrigin, region,
        srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, clearColorAllocation);
    executeBlitCommand(blitProperties, false);
    bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, QueueThrottle::MEDIUM);

    size_t dstOffset = 1 + dstOrigin[1] * dstRowPitch + dstOrigin[2] * dstSlicePitch;

    expectMemoryNotEqual<FamilyType>(ptrOffset(pDestMemory, dstOffset), pSrcMemory, copySize + 1, 0, 0);
    expectMemory<FamilyType>(ptrOffset(pDestMemory, dstOffset), pSrcMemory, copySize, 0, 0);
}
