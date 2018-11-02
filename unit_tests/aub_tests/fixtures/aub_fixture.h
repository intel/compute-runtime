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
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_interface.h"

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/tests_configuration.h"

#include <sstream>

namespace OCLRT {

class AUBFixture : public CommandQueueHwFixture {
  public:
    void SetUp(const HardwareInfo *hardwareInfo) {
        const HardwareInfo &hwInfo = hardwareInfo ? *hardwareInfo : *platformDevices[0];
        uint32_t deviceIndex = 0;

        auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);
        EngineType engineType = getChosenEngineType(hwInfo);

        const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::stringstream strfilename;
        strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

        executionEnvironment = new ExecutionEnvironment;

        if (testMode == TestMode::AubTests) {
            this->csr = AUBCommandStreamReceiver::create(hwInfo, strfilename.str(), true, *executionEnvironment);
        } else if (testMode == TestMode::AubTestsWithTbx) {
            this->csr = TbxCommandStreamReceiver::create(hwInfo, true, *executionEnvironment);
        }

        executionEnvironment->commandStreamReceivers.resize(deviceIndex + 1);
        executionEnvironment->commandStreamReceivers[deviceIndex] = std::unique_ptr<CommandStreamReceiver>(this->csr);
        device.reset(MockDevice::create<MockDevice>(&hwInfo, executionEnvironment, deviceIndex));

        CommandQueueHwFixture::SetUp(AUBFixture::device.get(), cl_command_queue_properties(0));
    }
    void TearDown() override {
        CommandQueueHwFixture::TearDown();
    }

    template <typename FamilyType>
    AUBCommandStreamReceiverHw<FamilyType> *getAubCsr() {
        AUBCommandStreamReceiverHw<FamilyType> *aubCsr = nullptr;
        if (testMode == TestMode::AubTests) {
            aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
        } else if (testMode == TestMode::AubTestsWithTbx) {
            aubCsr = reinterpret_cast<AUBCommandStreamReceiverHw<FamilyType> *>(
                reinterpret_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR);
        }
        return aubCsr;
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        auto aubCsr = getAubCsr<FamilyType>();
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

    static void *getGpuPointer(GraphicsAllocation *allocation) {
        return reinterpret_cast<void *>(allocation->getGpuAddress());
    }

    CommandStreamReceiver *csr = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    std::unique_ptr<MockDevice> device;

    ExecutionEnvironment *executionEnvironment;

  private:
    using CommandQueueHwFixture::SetUp;
};

template <typename KernelFixture>
struct KernelAUBFixture : public AUBFixture,
                          public KernelFixture {
    void SetUp() {
        AUBFixture::SetUp(nullptr);
        KernelFixture::SetUp(device.get(), context);
    }

    void TearDown() {
        KernelFixture::TearDown();
        AUBFixture::TearDown();
    }
};
} // namespace OCLRT
