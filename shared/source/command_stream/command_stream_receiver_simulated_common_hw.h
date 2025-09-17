/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/memory_banks.h"

#include "aub_mapper_common.h"
#include "aubstream/hardware_context.h"

namespace aub_stream {
class AubManager;
struct AubStream;
} // namespace aub_stream

namespace NEO {
class AddressMapper;
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedCommonHw : public CommandStreamReceiverHw<GfxFamily> {
  protected:
    using CommandStreamReceiverHw<GfxFamily>::osContext;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using MiContextDescriptorReg = typename AUB::MiContextDescriptorReg;

    bool getParametersForMemory(GraphicsAllocation &graphicsAllocation, uint64_t &gpuAddress, void *&cpuAddress, size_t &size) const;
    MOCKABLE_VIRTUAL uint32_t getDeviceIndex() const;

  public:
    using CommandStreamReceiverHw<GfxFamily>::peekExecutionEnvironment;
    using CommandStreamReceiverHw<GfxFamily>::writeMemory;
    using CommandStreamReceiverHw<GfxFamily>::pollForCompletion;

    CommandStreamReceiverSimulatedCommonHw(ExecutionEnvironment &executionEnvironment,
                                           uint32_t rootDeviceIndex,
                                           const DeviceBitfield deviceBitfield);
    ~CommandStreamReceiverSimulatedCommonHw() override;
    uint64_t getGTTBits() const {
        return 0u;
    }
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;
    static const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType);
    void setupContext(OsContext &osContext) override;
    virtual bool expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length);
    virtual bool expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length);
    virtual bool expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length);
    virtual void pollForCompletionImpl(){};
    virtual void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) = 0;
    virtual void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) = 0;
    virtual void writeMMIO(uint32_t offset, uint32_t value) = 0;
    void writeMemoryAub(aub_stream::AllocationParams &allocationParams) override {
        UNRECOVERABLE_IF(nullptr == hardwareContextController);
        hardwareContextController->writeMemory(allocationParams);
    }

    virtual void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool isAubWritable(GraphicsAllocation &graphicsAllocation) const = 0;
    virtual void setTbxWritable(bool writable, GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool isTbxWritable(GraphicsAllocation &graphicsAllocation) const = 0;

    virtual void dumpAllocation(GraphicsAllocation &gfxAllocation) = 0;

    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    size_t getPreferredTagPoolSize() const override { return 1; }

    aub_stream::AubManager *aubManager = nullptr;
    std::unique_ptr<HardwareContextController> hardwareContextController;
    ReleaseHelper *releaseHelper = nullptr;

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRingBuffer;
        uint32_t ggttRingBuffer;
        size_t sizeRingBuffer;
        uint32_t tailRingBuffer;
    } engineInfo = {};
};
} // namespace NEO
