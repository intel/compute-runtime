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
        if (testMode == TestMode::AubTestsWithTbx) {
            this->csr = TbxCommandStreamReceiver::create(hwInfo, true, *executionEnvironment);
        } else {
            this->csr = AUBCommandStreamReceiver::create(hwInfo, strfilename.str(), true, *executionEnvironment);
        }

        executionEnvironment->commandStreamReceivers.resize(deviceIndex + 1);

        device.reset(MockDevice::create<MockDevice>(&hwInfo, executionEnvironment, deviceIndex));
        device->resetCommandStreamReceiver(this->csr);

        CommandQueueHwFixture::SetUp(AUBFixture::device.get(), cl_command_queue_properties(0));
    }
    void TearDown() override {
        CommandQueueHwFixture::TearDown();
    }

    GraphicsAllocation *createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size);

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
        aubCsr->expectMemoryEqual(gfxAddress, srcAddress, length);
    }

    template <typename FamilyType>
    void expectNotEqualMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        auto aubCsr = getAubCsr<FamilyType>();
        aubCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length);
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
