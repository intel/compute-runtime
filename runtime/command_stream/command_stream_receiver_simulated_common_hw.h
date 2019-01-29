/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/memory_manager/memory_banks.h"
#include "third_party/aub_stream/headers/hardware_context.h"

namespace aub_stream {
class AubManager;
struct AubStream;
} // namespace aub_stream

namespace OCLRT {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedCommonHw : public CommandStreamReceiverHw<GfxFamily> {
  protected:
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<GfxFamily>::deviceIndex;
    using CommandStreamReceiverHw<GfxFamily>::hwInfo;
    using CommandStreamReceiverHw<GfxFamily>::osContext;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using MiContextDescriptorReg = typename AUB::MiContextDescriptorReg;

    uint32_t engineIndex = 0;

  public:
    uint64_t getGTTBits() const {
        return 0u;
    }
    void initGlobalMMIO();
    void initAdditionalMMIO();
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;
    uint32_t getEngineIndex(EngineInstanceT engineInstance);
    static const AubMemDump::LrcaHelper &getCsTraits(EngineInstanceT engineInstance);
    void initEngineMMIO();
    void submitLRCA(const MiContextDescriptorReg &contextDescriptor);
    void setupContext(OsContext &osContext) override;

    aub_stream::AubManager *aubManager = nullptr;
    std::unique_ptr<aub_stream::HardwareContext> hardwareContext;

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRingBuffer;
        uint32_t ggttRingBuffer;
        size_t sizeRingBuffer;
        uint32_t tailRingBuffer;
    } engineInfoTable[EngineInstanceConstants::numAllEngineInstances] = {};

    AubMemDump::AubStream *stream;
};
} // namespace OCLRT
