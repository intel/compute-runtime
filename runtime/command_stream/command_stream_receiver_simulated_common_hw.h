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
#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/hardware_context.h"

using namespace AubDump;

namespace AubMemDump {
struct AubStream;
}

namespace OCLRT {
class GraphicsAllocation;
template <typename GfxFamily>
class CommandStreamReceiverSimulatedCommonHw : public CommandStreamReceiverHw<GfxFamily> {
  protected:
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<GfxFamily>::osContext;
    using AUB = typename AUBFamilyMapper<GfxFamily>::AUB;
    using MiContextDescriptorReg = typename AUB::MiContextDescriptorReg;

  public:
    uint64_t getGTTBits() const {
        return 0u;
    }
    void initGlobalMMIO();
    void initAdditionalMMIO();
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);
    uint32_t getMemoryBankForGtt() const;
    size_t getEngineIndex(EngineInstanceT engineInstance);
    static const AubMemDump::LrcaHelper &getCsTraits(EngineInstanceT engineInstance);
    void initEngineMMIO(EngineInstanceT engineInstance);
    void submitLRCA(EngineInstanceT engineInstance, const MiContextDescriptorReg &contextDescriptor);

    AubManager *aubManager = nullptr;
    std::unique_ptr<HardwareContext> hardwareContext;

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
    size_t gpgpuEngineIndex = EngineInstanceConstants::numGpgpuEngineInstances - 1;

  protected:
    MMIOList splitMMIORegisters(const std::string &registers, char delimiter);
};
} // namespace OCLRT
