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
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/memory_manager/address_mapper.h"
#include "runtime/memory_manager/page_table.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"

namespace OCLRT {

class AubSubCaptureManager;

template <typename GfxFamily>
class AUBCommandStreamReceiverHw : public CommandStreamReceiverHw<GfxFamily> {
    typedef CommandStreamReceiverHw<GfxFamily> BaseClass;
    typedef typename AUBFamilyMapper<GfxFamily>::AUB AUB;
    typedef typename AUB::MiContextDescriptorReg MiContextDescriptorReg;

  public:
    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency) override;
    void makeResident(GraphicsAllocation &gfxAllocation) override;
    void makeNonResident(GraphicsAllocation &gfxAllocation) override;

    void processResidency(ResidencyContainer *allocationsForResidency) override;

    MOCKABLE_VIRTUAL bool writeMemory(GraphicsAllocation &gfxAllocation);

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override;

    // Family specific version
    void submitLRCA(EngineType engineType, const MiContextDescriptorReg &contextDescriptor);
    void pollForCompletion(EngineType engineType);
    void initGlobalMMIO();
    void initEngineMMIO(EngineType engineType);

    void addContextToken();

    static CommandStreamReceiver *create(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone);

    AUBCommandStreamReceiverHw(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone);
    ~AUBCommandStreamReceiverHw() override;

    AUBCommandStreamReceiverHw(const AUBCommandStreamReceiverHw &) = delete;
    AUBCommandStreamReceiverHw &operator=(const AUBCommandStreamReceiverHw &) = delete;

    MOCKABLE_VIRTUAL void initFile(const std::string &fileName);
    MOCKABLE_VIRTUAL void closeFile();
    MOCKABLE_VIRTUAL bool isFileOpen();
    MOCKABLE_VIRTUAL const std::string &getFileName();

    void initializeEngine(EngineType engineType);
    void freeEngineInfoTable();

    MemoryManager *createMemoryManager(bool enable64kbPages) override {
        this->memoryManager = new OsAgnosticMemoryManager(enable64kbPages);
        this->flatBatchBufferHelper->setMemoryManager(this->memoryManager);
        return this->memoryManager;
    }

    static const AubMemDump::LrcaHelper &getCsTraits(EngineType engineType);

    struct EngineInfo {
        void *pLRCA;
        uint32_t ggttLRCA;
        void *pGlobalHWStatusPage;
        uint32_t ggttHWSP;
        void *pRingBuffer;
        uint32_t ggttRingBuffer;
        size_t sizeRingBuffer;
        uint32_t tailRingBuffer;
    } engineInfoTable[EngineType::NUM_ENGINES] = {};

    std::unique_ptr<AUBCommandStreamReceiver::AubFileStream> stream;
    std::unique_ptr<AubSubCaptureManager> subCaptureManager;
    uint32_t aubDeviceId;
    bool standalone;

    TypeSelector<PML4, PDPE, sizeof(void *) == 8>::type ppgtt;
    PDPE ggtt;
    // remap CPU VA -> GGTT VA
    AddressMapper gttRemap;

    MOCKABLE_VIRTUAL bool addPatchInfoComments();
    void addGUCStartMessage(uint64_t batchBufferAddress, EngineType engineType);
    uint32_t getGUCWorkQueueItemHeader(EngineType engineType);
    uint64_t getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation);
    void getGTTData(void *memory, AubGTTData &data);

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_AUB;
    }

  protected:
    bool dumpAubNonWritable = false;
};
} // namespace OCLRT
