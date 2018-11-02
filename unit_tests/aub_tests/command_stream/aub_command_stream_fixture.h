/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_banks.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/tests_configuration.h"

#include <cstdint>

namespace OCLRT {
class CommandStreamReceiver;

class AUBCommandStreamFixture : public CommandStreamFixture {
  public:
    virtual void SetUp(CommandQueue *pCommandQueue);
    virtual void TearDown();

    template <typename FamilyType>
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        using AubMemDump::CmdServicesMemTraceRegisterCompare;
        CmdServicesMemTraceRegisterCompare header;
        memset(&header, 0, sizeof(header));
        header.setHeader();

        header.data[0] = expectedValue;
        header.registerOffset = mmioRegister;
        header.noReadExpect = CmdServicesMemTraceRegisterCompare::NoReadExpectValues::ReadExpect;
        header.registerSize = CmdServicesMemTraceRegisterCompare::RegisterSizeValues::Dword;
        header.registerSpace = CmdServicesMemTraceRegisterCompare::RegisterSpaceValues::Mmio;
        header.readMaskLow = 0xffffffff;
        header.readMaskHigh = 0xffffffff;
        header.dwordCount = (sizeof(header) / sizeof(uint32_t)) - 1;

        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            csr = reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver)->aubCSR;
        }

        // Write our pseudo-op to the AUB file
        auto aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
        aubCsr->getAubStream()->fileHandle.write(reinterpret_cast<char *>(&header), sizeof(header));
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            csr = reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver)->aubCSR;
        }

        auto aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            if (offset > length)
                abort();

            aubCsr->getAubStream()->expectMemory(physAddress,
                                                 reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(srcAddress) + offset),
                                                 size,
                                                 aubCsr->getAddressSpaceFromPTEBits(entryBits));
        };

        aubCsr->ppgtt->pageWalk(reinterpret_cast<uintptr_t>(gfxAddress), length, 0, PageTableEntry::nonValidBits, walker, MemoryBanks::BankNotSpecified);
    }

    GraphicsAllocation *createResidentAllocationAndStoreItInCsr(const void *address, size_t size) {
        GraphicsAllocation *graphicsAllocation = pCommandStreamReceiver->getMemoryManager()->allocateGraphicsMemory(size, address);
        pCommandStreamReceiver->makeResidentHostPtrAllocation(graphicsAllocation);
        pCommandStreamReceiver->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(graphicsAllocation), TEMPORARY_ALLOCATION);
        return graphicsAllocation;
    }
    CommandStreamReceiver *pCommandStreamReceiver = nullptr;
    volatile uint32_t *pTagMemory = nullptr;

  private:
    CommandQueue *commandQueue = nullptr;
};
} // namespace OCLRT
