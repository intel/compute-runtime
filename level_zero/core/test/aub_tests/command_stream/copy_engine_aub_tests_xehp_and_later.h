/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/aub_tests/fixtures/multicontext_l0_aub_fixture.h"

#include "gtest/gtest.h"

#include <functional>
#include <memory>
#include <vector>

namespace L0 {
namespace ult {

template <uint32_t numTiles, bool useLocalMemory>
struct CopyEngineXeHPAndLater : public MulticontextL0AubFixture, public ::testing::Test {
    using MulticontextL0AubFixture::expectMemory;
    using BufferPtr = std::unique_ptr<NEO::GraphicsAllocation, std::function<void(NEO::GraphicsAllocation *)>>;

    void SetUp() override {
        NEO::MockExecutionEnvironment mockExecutionEnvironment{};
        auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->template getHelper<NEO::ProductHelper>();
        if (!productHelper.obtainBlitterPreference(*NEO::defaultHwInfo.get())) {
            GTEST_SKIP();
        }
        this->bcsEngineType = this->getBcsEngineTypeToUse(productHelper);

        if (useLocalMemory) {
            if (!NEO::defaultHwInfo->featureTable.flags.ftrLocalMemory) {
                GTEST_SKIP();
            }
            NEO::debugManager.flags.EnableLocalMemory.set(true);
        }

        NEO::debugManager.flags.EnableFreeMemory.set(false);

        NEO::MulticontextAubFixture::setUp(numTiles, EnabledCommandStreamers::single, true);
        if (this->skipped) {
            GTEST_SKIP();
        }

        this->bcsCsr = this->rootDevice->getNEODevice()->getNearestGenericSubDevice(0)->getEngine(this->bcsEngineType, NEO::EngineUsage::regular).commandStreamReceiver;

        this->compressiblePattern = std::make_unique<uint8_t[]>(bufferSize);
        std::fill(this->compressiblePattern.get(), ptrOffset(this->compressiblePattern.get(), bufferSize), 0xC6);

        this->writePattern = std::make_unique<uint8_t[]>(bufferSize);
        this->dstHostPtr = std::make_unique<uint8_t[]>(bufferSize);
        for (size_t i = 0; i < bufferSize; i++) {
            this->writePattern.get()[i] = static_cast<uint8_t>(i);
            this->dstHostPtr.get()[i] = 255 - this->writePattern[i];
        }

        EXPECT_NE(this->writePattern.get()[0], this->writePattern.get()[offset]);
    }

    void TearDown() override {
        MulticontextL0AubFixture::tearDown();
    }

    NEO::CommandStreamReceiver *getGpgpuCsr(uint32_t tile, uint32_t engine) override {
        return this->rootDevice->getNEODevice()->getNearestGenericSubDevice(tile)->getDefaultEngine().commandStreamReceiver;
    }

    virtual bool compressionSupported() const {
        auto &ftrTable = this->rootDevice->getNEODevice()->getHardwareInfo().featureTable;
        if constexpr (useLocalMemory) {
            return (ftrTable.flags.ftrLocalMemory && ftrTable.flags.ftrFlatPhysCCS);
        } else {
            return (!ftrTable.flags.ftrLocalMemory && ftrTable.flags.ftrFlatPhysCCS && ftrTable.flags.ftrXe2Compression);
        }
    }

    virtual aub_stream::EngineType getBcsEngineTypeToUse(const NEO::ProductHelper &productHelper) const {
        return productHelper.getDefaultCopyEngine();
    }

    BufferPtr createBuffer(bool compressed, bool inLocalMemory, void *srcHostPtr) {
        auto neoDevice = this->rootDevice->getNEODevice();
        auto memoryManager = neoDevice->getMemoryManager();
        auto allocationType = (!inLocalMemory && !compressed) ? NEO::AllocationType::bufferHostMemory : NEO::AllocationType::buffer;

        NEO::AllocationProperties properties{this->rootDeviceIndex, true, bufferSize, allocationType, false, neoDevice->getDeviceBitfield()};
        properties.flags.preferCompressed = compressed;
        properties.flags.isHostInaccessibleAllocation = compressed;

        BufferPtr buffer(memoryManager->allocateGraphicsMemoryWithProperties(properties), [memoryManager](NEO::GraphicsAllocation *allocation) { memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(allocation); });
        EXPECT_NE(nullptr, buffer);
        if (!buffer) {
            return buffer;
        }

        if (compressed) {
            EXPECT_TRUE(buffer->isCompressionEnabled());
        }
        EXPECT_EQ(!inLocalMemory, !buffer->isAllocatedInLocalMemoryPool());

        if (srcHostPtr) {
            EXPECT_TRUE(NEO::MemoryTransferHelper::transferMemoryToAllocation(buffer->isCompressionEnabled(), *neoDevice, buffer.get(), 0, srcHostPtr, bufferSize));
        }

        return buffer;
    }

    void *getGpuAddress(NEO::GraphicsAllocation &buffer) {
        return addrToPtr(this->getGpuVA(buffer));
    }

    uint64_t getGpuVA(NEO::GraphicsAllocation &buffer) {
        return buffer.getGpuAddress();
    }

    static size_t calculateHostPtrSize(const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch) {
        size_t hostPtrOffsetInBytes = origin[2] * slicePitch + origin[1] * rowPitch + origin[0];
        size_t hostPtrRegionSizeInbytes = region[0] + rowPitch * (region[1] - 1) + slicePitch * (region[2] - 1);
        return hostPtrOffsetInBytes + hostPtrRegionSizeInbytes;
    }

    void executeBlitCommand(const NEO::BlitProperties &blitProperties, bool blocking) {
        NEO::BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);
        this->bcsCsr->flushBcsTask(blitPropertiesContainer, blocking, *this->rootDevice->getNEODevice());
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
    void givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();
    template <typename FamilyType>
    void givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl();

    NEO::CommandStreamReceiver *bcsCsr = nullptr;
    static constexpr size_t bufferSize = MemoryConstants::pageSize64k + BlitterConstants::maxBlitWidth + 3;
    static constexpr size_t offset = (bufferSize / 4) - 3;
    aub_stream::EngineType bcsEngineType = aub_stream::EngineType::ENGINE_BCS;

    std::unique_ptr<uint8_t[]> compressiblePattern;
    std::unique_ptr<uint8_t[]> writePattern;
    std::unique_ptr<uint8_t[]> dstHostPtr;
};

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl() {
    if (!this->compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcNotCompressedBuffer = this->createBuffer(false, testLocalMemory, this->compressiblePattern.get());
    auto dstNotCompressedBuffer = this->createBuffer(false, testLocalMemory, nullptr);
    auto dstCompressedBuffer = this->createBuffer(true, testLocalMemory, nullptr);
    auto dstResolvedBuffer = this->createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstCompressedBuffer.get(), 0,
        srcNotCompressedBuffer.get(), 0,
        {0, 0, 0}, {0, 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);
    this->executeBlitCommand(blitProperties, true);
    blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstNotCompressedBuffer.get(), 0,
        srcNotCompressedBuffer.get(), 0,
        {0, 0, 0}, {0, 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);
    this->executeBlitCommand(blitProperties, true);
    blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstResolvedBuffer.get(), 0,
        dstCompressedBuffer.get(), 0,
        {0, 0, 0}, {0, 0, 0}, {bufferSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);
    this->executeBlitCommand(blitProperties, true);

    blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *this->bcsCsr,
                                                                          dstCompressedBuffer.get(), nullptr,
                                                                          this->dstHostPtr.get(),
                                                                          this->getGpuVA(*dstCompressedBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    this->executeBlitCommand(blitProperties, true);
    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*dstCompressedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->getGpuAddress(*dstResolvedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->getGpuAddress(*dstNotCompressedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->getGpuAddress(*srcNotCompressedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!this->compressionSupported()) {
        GTEST_SKIP();
    }

    auto dstCompressedBuffer = this->createBuffer(true, testLocalMemory, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr, dstCompressedBuffer.get(), nullptr,
                                                                               this->compressiblePattern.get(),
                                                                               this->getGpuVA(*dstCompressedBuffer), 0,
                                                                               0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, true);

    blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *this->bcsCsr,
                                                                          dstCompressedBuffer.get(), nullptr,
                                                                          this->dstHostPtr.get(),
                                                                          this->getGpuVA(*dstCompressedBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    this->executeBlitCommand(blitProperties, true);

    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*dstCompressedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!this->compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcCompressedBuffer = this->createBuffer(true, testLocalMemory, nullptr);
    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr, srcCompressedBuffer.get(),
                                                                               nullptr, this->compressiblePattern.get(),
                                                                               this->getGpuVA(*srcCompressedBuffer), 0,
                                                                               0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    this->executeBlitCommand(blitProperties, true);

    blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                          *this->bcsCsr, srcCompressedBuffer.get(),
                                                                          nullptr, this->dstHostPtr.get(),
                                                                          this->getGpuVA(*srcCompressedBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*srcCompressedBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto srcNotCompressedLocalBuffer = this->createBuffer(false, testLocalMemory, this->compressiblePattern.get());

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                               *this->bcsCsr, srcNotCompressedLocalBuffer.get(),
                                                                               nullptr, this->dstHostPtr.get(),
                                                                               this->getGpuVA(*srcNotCompressedLocalBuffer), 0,
                                                                               0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    this->expectMemory<FamilyType>(this->getGpuAddress(*srcNotCompressedLocalBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto buffer = this->createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr, buffer.get(), nullptr, this->compressiblePattern.get(),
                                                                               this->getGpuVA(*buffer), 0,
                                                                               0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, true);

    this->expectMemory<FamilyType>(this->getGpuAddress(*buffer), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto buffer = this->createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr, buffer.get(), nullptr, this->writePattern.get(),
                                                                               this->getGpuVA(*buffer), 0,
                                                                               0, {offset, 0, 0}, {bufferSize - offset, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, true);

    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*buffer), this->writePattern.get(), bufferSize, 0, 0);
    this->expectMemoryNotEqual<FamilyType>(ptrOffset(this->getGpuAddress(*buffer), offset - 1), this->writePattern.get(), bufferSize - offset, 0, 0);
    this->expectMemory<FamilyType>(ptrOffset(this->getGpuAddress(*buffer), offset), this->writePattern.get(), bufferSize - offset, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    auto srcBuffer = this->createBuffer(false, testLocalMemory, this->writePattern.get());

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                               *this->bcsCsr, srcBuffer.get(), nullptr, this->dstHostPtr.get(),
                                                                               this->getGpuVA(*srcBuffer), 0,
                                                                               0, {offset, 0, 0}, {bufferSize - offset, 1, 1}, 0, 0, 0, 0);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    this->expectMemoryNotEqual<FamilyType>(this->dstHostPtr.get(), this->writePattern.get(), offset, 0, 0);
    this->expectMemoryNotEqual<FamilyType>(ptrOffset(this->dstHostPtr.get(), 1), ptrOffset(this->writePattern.get(), offset), bufferSize - offset - 1, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), ptrOffset(this->writePattern.get(), offset), bufferSize - offset, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl() {
    size_t copiedSize = bufferSize - (2 * offset);

    auto srcBuffer = this->createBuffer(false, testLocalMemory, this->writePattern.get());
    auto dstBuffer = this->createBuffer(false, testLocalMemory, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstBuffer.get(), 0,
        srcBuffer.get(), 0,
        {offset, 0, 0}, {0, 0, 0}, {copiedSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);

    this->executeBlitCommand(blitProperties, true);

    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*dstBuffer), this->writePattern.get(), bufferSize, 0, 0);
    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*dstBuffer), this->writePattern.get(), copiedSize + 1, 0, 0);
    this->expectMemoryNotEqual<FamilyType>(ptrOffset(this->getGpuAddress(*dstBuffer), offset - 1), this->writePattern.get(), copiedSize, 0, 0);
    this->expectMemory<FamilyType>(ptrOffset(this->getGpuAddress(*dstBuffer), offset), this->writePattern.get(), copiedSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl() {
    if (!this->compressionSupported()) {
        GTEST_SKIP();
    }

    auto srcBuffer = this->createBuffer(true, testLocalMemory, nullptr);
    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr, srcBuffer.get(),
                                                                               nullptr, this->compressiblePattern.get(),
                                                                               this->getGpuVA(*srcBuffer), 0,
                                                                               0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    this->executeBlitCommand(blitProperties, false);
    auto dstBuffer = this->createBuffer(true, testLocalMemory, nullptr);

    blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstBuffer.get(), 0,
        srcBuffer.get(), 0,
        0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);

    this->executeBlitCommand(blitProperties, true);

    blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr, *this->bcsCsr,
                                                                          dstBuffer.get(), nullptr,
                                                                          this->dstHostPtr.get(),
                                                                          this->getGpuVA(*dstBuffer), 0,
                                                                          0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0);
    this->executeBlitCommand(blitProperties, true);

    this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*dstBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    this->expectMemory<FamilyType>(this->dstHostPtr.get(), this->compressiblePattern.get(), bufferSize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl() {
    auto hwInfo = this->rootDevice->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    if ((hwInfo->capabilityTable.sharedSystemMemCapabilities > 0) || !this->compressionSupported() ||
        (NEO::GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(*hwInfo) != NEO::AuxTranslationMode::blit)) {
        GTEST_SKIP();
    }

    auto buffer = this->createBuffer(true, testLocalMemory, this->compressiblePattern.get());

    {
        this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*buffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    }

    {
        auto blitProperties = NEO::BlitProperties::constructPropertiesForAuxTranslation(NEO::AuxTranslationDirection::auxToNonAux,
                                                                                        buffer.get(),
                                                                                        this->bcsCsr->getClearColorAllocation());

        this->executeBlitCommand(blitProperties, false);
        this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

        this->expectMemory<FamilyType>(this->getGpuAddress(*buffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    }

    {
        auto blitProperties = NEO::BlitProperties::constructPropertiesForAuxTranslation(NEO::AuxTranslationDirection::nonAuxToAux,
                                                                                        buffer.get(),
                                                                                        this->bcsCsr->getClearColorAllocation());

        this->executeBlitCommand(blitProperties, false);
        this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

        this->expectMemoryNotEqual<FamilyType>(this->getGpuAddress(*buffer), this->compressiblePattern.get(), bufferSize, 0, 0);
    }
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl() {
    auto srcBuffer = this->createBuffer(false, false, this->compressiblePattern.get());
    auto dstBuffer = this->createBuffer(false, false, nullptr);

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstBuffer.get(), 0,
        srcBuffer.get(), 0,
        0, 0, {bufferSize, 1, 1}, 0, 0, 0, 0, this->bcsCsr->getClearColorAllocation(), false);

    this->executeBlitCommand(blitProperties, true);

    this->expectMemory<FamilyType>(this->getGpuAddress(*dstBuffer), this->compressiblePattern.get(), bufferSize, 0, 0);
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

    auto srcBuffer = this->createBuffer(false, testLocalMemory, srcMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    size_t hostOrigin[] = {0, 0, 0};
    size_t bufferOrigin[] = {1 * sizeof(uint8_t), 1, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 1};
    size_t hostRowPitch = 2 * sizeof(uint8_t);
    size_t hostSlicePitch = 4 * sizeof(uint8_t);
    size_t bufferRowPitch = 4 * sizeof(uint8_t);
    size_t bufferSlicePitch = 8 * sizeof(uint8_t);

    size_t hostPtrSize = this->calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);

    NEO::HostPtrSurface hostPtrSurface(destMemory.get(), hostPtrSize, true);
    this->bcsCsr->createAllocationForHostSurface(hostPtrSurface, true);
    NEO::GraphicsAllocation *allocation = hostPtrSurface.getAllocation();
    auto srcAllocation = srcBuffer.get();

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::bufferToHostPtr,
                                                                               *this->bcsCsr,
                                                                               srcAllocation,
                                                                               allocation,
                                                                               pDestMemory,
                                                                               this->getGpuVA(*srcBuffer),
                                                                               allocation->getGpuAddress(),
                                                                               hostOrigin,
                                                                               bufferOrigin,
                                                                               region,
                                                                               hostRowPitch,
                                                                               hostSlicePitch,
                                                                               bufferRowPitch,
                                                                               bufferSlicePitch);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    int *destGpuAddress = reinterpret_cast<int *>(allocation->getGpuAddress());

    this->expectMemoryNotEqual<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize + 1, 0, 0);
    this->expectMemory<FamilyType>(destGpuAddress, pSrcMemory, hostPtrSize, 0, 0);
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

    auto srcBuffer = this->createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = &destMemory[0];

    size_t hostOrigin[] = {0, 0, 0};
    size_t bufferOrigin[] = {1 * sizeof(uint8_t), 1, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 1};
    size_t hostRowPitch = 2 * sizeof(uint8_t);
    size_t hostSlicePitch = 4 * sizeof(uint8_t);
    size_t bufferRowPitch = 4 * sizeof(uint8_t);
    size_t bufferSlicePitch = 8 * sizeof(uint8_t);

    size_t hostPtrSize = this->calculateHostPtrSize(hostOrigin, region, hostRowPitch, hostSlicePitch);

    NEO::HostPtrSurface hostPtrSurface(srcMemory.get(), hostPtrSize, true);
    this->bcsCsr->createAllocationForHostSurface(hostPtrSurface, true);
    NEO::GraphicsAllocation *allocation = hostPtrSurface.getAllocation();
    auto srcAllocation = srcBuffer.get();

    auto blitProperties = NEO::BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                               *this->bcsCsr,
                                                                               srcAllocation,
                                                                               allocation,
                                                                               pDestMemory,
                                                                               this->getGpuVA(*srcBuffer),
                                                                               allocation->getGpuAddress(),
                                                                               hostOrigin,
                                                                               bufferOrigin,
                                                                               region,
                                                                               hostRowPitch,
                                                                               hostSlicePitch,
                                                                               bufferRowPitch,
                                                                               bufferSlicePitch);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    std::vector<uint8_t> expectedBufferMemory(destMemory.get(), destMemory.get() + (bufferOrigin[1] + region[1]) * bufferRowPitch);
    for (size_t row = 0; row < region[1]; row++) {
        for (size_t column = 0; column < region[0]; column++) {
            expectedBufferMemory[(bufferOrigin[1] + row) * bufferRowPitch + bufferOrigin[0] + column] = pSrcMemory[(hostOrigin[1] + row) * hostRowPitch + hostOrigin[0] + column];
        }
    }
    this->expectMemory<FamilyType>(this->getGpuAddress(*srcBuffer), expectedBufferMemory.data(), expectedBufferMemory.size(), 0, 0);
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

    auto srcBuffer = this->createBuffer(false, testLocalMemory, srcMemory.get());
    auto dstBuffer = this->createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = reinterpret_cast<uint8_t *>(this->getGpuAddress((*dstBuffer)));
    auto clearColorAllocation = this->bcsCsr->getClearColorAllocation();

    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {1 * sizeof(uint8_t), 0, 0};
    size_t region[] = {2 * sizeof(uint8_t), 2, 2};
    size_t srcRowPitch = region[0];
    size_t srcSlicePitch = srcRowPitch * region[1];
    size_t dstRowPitch = region[0];
    size_t dstSlicePitch = dstRowPitch * region[1];
    auto copySize = region[0] * region[1] * region[2];

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstBuffer.get(), 0,
        srcBuffer.get(), 0,
        dstOrigin, srcOrigin, region,
        srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, clearColorAllocation, false);

    this->executeBlitCommand(blitProperties, false);

    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    this->expectMemoryNotEqual<FamilyType>(ptrOffset(pDestMemory, sizeof(uint8_t)), pSrcMemory, copySize + 1, 0, 0);
    this->expectMemory<FamilyType>(ptrOffset(pDestMemory, sizeof(uint8_t)), pSrcMemory, copySize, 0, 0);
}

template <uint32_t numTiles, bool testLocalMemory>
template <typename FamilyType>
void CopyEngineXeHPAndLater<numTiles, testLocalMemory>::givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl() {
    NEO::debugManager.flags.LimitBlitterMaxWidth.set(8);
    NEO::debugManager.flags.LimitBlitterMaxHeight.set(8);

    auto srcMemory = std::make_unique<uint8_t[]>(bufferSize);
    auto destMemory = std::make_unique<uint8_t[]>(bufferSize);

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = static_cast<uint8_t>(1);
        destMemory[i] = static_cast<uint8_t>(2);
    }

    auto srcBuffer = this->createBuffer(false, testLocalMemory, srcMemory.get());
    auto dstBuffer = this->createBuffer(false, testLocalMemory, destMemory.get());
    auto pSrcMemory = &srcMemory[0];
    auto pDestMemory = reinterpret_cast<uint8_t *>(this->getGpuAddress(*dstBuffer));
    auto clearColorAllocation = this->bcsCsr->getClearColorAllocation();

    size_t srcOrigin[] = {0, 0, 0};
    size_t dstOrigin[] = {1, 1, 1};
    size_t region[] = {20, 16, 2};
    size_t srcRowPitch = region[0];
    size_t srcSlicePitch = srcRowPitch * region[1];
    size_t dstRowPitch = region[0];
    size_t dstSlicePitch = dstRowPitch * region[1];
    auto copySize = region[0] * region[1] * region[2];

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(
        dstBuffer.get(), 0,
        srcBuffer.get(), 0,
        dstOrigin, srcOrigin, region,
        srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch, clearColorAllocation, false);
    this->executeBlitCommand(blitProperties, false);
    this->bcsCsr->waitForTaskCountWithKmdNotifyFallback(0, 0, false, NEO::QueueThrottle::MEDIUM);

    size_t dstOffset = 1 + dstOrigin[1] * dstRowPitch + dstOrigin[2] * dstSlicePitch;

    this->expectMemoryNotEqual<FamilyType>(ptrOffset(pDestMemory, dstOffset), pSrcMemory, copySize + 1, 0, 0);
    this->expectMemory<FamilyType>(ptrOffset(pDestMemory, dstOffset), pSrcMemory, copySize, 0, 0);
}

} // namespace ult
} // namespace L0
