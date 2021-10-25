/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/memory_manager/memory_banks.h"

#include "aub_mapper.h"
#include "third_party/aub_stream/headers/hardware_context.h"

namespace aub_stream {
class AubManager;
struct AubStream;
} // namespace aub_stream

namespace NEO {
class AddressMapper;
class GraphicsAllocation;
class HardwareContextController;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedCommonHw : public CommandStreamReceiverHw<GfxFamily> {
  protected:
    using CommandStreamReceiverHw<GfxFamily>::osContext;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using MiContextDescriptorReg = typename AUB::MiContextDescriptorReg;

    bool getParametersForWriteMemory(GraphicsAllocation &graphicsAllocation, uint64_t &gpuAddress, void *&cpuAddress, size_t &size) const;
    void freeEngineInfo(AddressMapper &gttRemap);
    MOCKABLE_VIRTUAL uint32_t getDeviceIndex() const;

  public:
    CommandStreamReceiverSimulatedCommonHw(ExecutionEnvironment &executionEnvironment,
                                           uint32_t rootDeviceIndex,
                                           const DeviceBitfield deviceBitfield);
    ~CommandStreamReceiverSimulatedCommonHw() override;
    uint64_t getGTTBits() const {
        return 0u;
    }
    void initGlobalMMIO();
    void initAdditionalMMIO();
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;
    static const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType);
    void initEngineMMIO();
    void submitLRCA(const MiContextDescriptorReg &contextDescriptor);
    void setupContext(OsContext &osContext) override;
    virtual bool expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length);
    virtual bool expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length);
    virtual bool expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length);
    virtual void pollForCompletionImpl(){};
    virtual bool writeMemory(GraphicsAllocation &gfxAllocation) = 0;
    virtual void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) = 0;
    virtual void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation) = 0;
    virtual void writeMMIO(uint32_t offset, uint32_t value) = 0;

    virtual void setAubWritable(bool writable, GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool isAubWritable(GraphicsAllocation &graphicsAllocation) const = 0;
    virtual void setTbxWritable(bool writable, GraphicsAllocation &graphicsAllocation) = 0;
    virtual bool isTbxWritable(GraphicsAllocation &graphicsAllocation) const = 0;

    virtual void dumpAllocation(GraphicsAllocation &gfxAllocation) = 0;
    virtual void initializeEngine() = 0;

    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    size_t getPreferredTagPoolSize() const override { return 1; }

    aub_stream::AubManager *aubManager = nullptr;
    std::unique_ptr<HardwareContextController> hardwareContextController;

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

    AubMemDump::AubStream *stream;
};
} // namespace NEO
