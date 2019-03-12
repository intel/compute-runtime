/*
 * Copyright (C) 2017-2019 Intel Corporation
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
#include "runtime/os_interface/os_context.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/tests_configuration.h"

#include <cstdint>

namespace OCLRT {
class CommandStreamReceiver;

class AUBCommandStreamFixture : public CommandStreamFixture {
  public:
    virtual void SetUp(CommandQueue *pCommandQueue);
    virtual void TearDown();

    template <typename FamilyType>
    AUBCommandStreamReceiverHw<FamilyType> *getAubCsr() const {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            csr = static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get();
        }
        return static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            csr = reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver)->aubCSR.get();
        }

        if (csr) {
            // Write our pseudo-op to the AUB file
            auto aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
            aubCsr->expectMMIO(mmioRegister, expectedValue);
        }
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = reinterpret_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(pCommandStreamReceiver);
            tbxCsr->expectMemoryEqual(gfxAddress, srcAddress, length);

            csr = reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver)->aubCSR.get();
        }

        if (csr) {
            auto aubCsr = reinterpret_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(csr);
            aubCsr->expectMemoryEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = reinterpret_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(pCommandStreamReceiver);
            tbxCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length);

            csr = reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(pCommandStreamReceiver)->aubCSR.get();
        }

        if (csr) {
            auto aubCsr = reinterpret_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(csr);
            aubCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr() {
        return reinterpret_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(pCommandStreamReceiver);
    }

    template <typename FamilyType>
    void pollForCompletion() {
        getSimulatedCsr<FamilyType>()->pollForCompletion();
    }

    GraphicsAllocation *createResidentAllocationAndStoreItInCsr(const void *address, size_t size) {
        GraphicsAllocation *graphicsAllocation = pCommandStreamReceiver->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, address);
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
