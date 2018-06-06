/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/memory_manager/page_table.h"

namespace OCLRT {

class TbxMemoryManager : public OsAgnosticMemoryManager {
  public:
    uint64_t getSystemSharedMemory() override {
        return 1 * GB;
    }
};

template <typename GfxFamily>
class TbxCommandStreamReceiverHw : public CommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::memoryManager;
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;
    typedef typename OCLRT::AUBFamilyMapper<GfxFamily>::AUB AUB;
    typedef typename AUB::MiContextDescriptorReg MiContextDescriptorReg;

  public:
    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency) override;
    void makeCoherent(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer *allocationsForResidency) override;
    bool writeMemory(GraphicsAllocation &gfxAllocation);

    // Family specific version
    void submitLRCA(EngineType engineType, const MiContextDescriptorReg &contextDescriptor);
    void pollForCompletion(EngineType engineType);
    void initGlobalMMIO();
    void initEngineMMIO(EngineType engineType);

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, bool withAubDump);

    TbxCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, void *ptr);
    ~TbxCommandStreamReceiverHw() override;

    void initializeEngine(EngineType engineType);

    static const AubMemDump::LrcaHelper &getCsTraits(EngineType engineType);

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRCS;
        uint32_t ggttRCS;
        size_t sizeRCS;
        uint32_t tailRCS;
    } engineInfoTable[EngineType::NUM_ENGINES];

    MemoryManager *createMemoryManager(bool enable64kbPages) override {
        memoryManager = new TbxMemoryManager;
        return memoryManager;
    }
    TbxMemoryManager *getMemoryManager() {
        return (TbxMemoryManager *)CommandStreamReceiver::getMemoryManager();
    }
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);

    TbxCommandStreamReceiver::TbxStream stream;
    uint32_t aubDeviceId;

    TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type ppgtt;
    PDPE ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper gttRemap;

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_TBX;
    }
};
} // namespace OCLRT
