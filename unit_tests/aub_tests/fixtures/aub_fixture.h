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
#include "runtime/platform/platform.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/tests_configuration.h"

#include <sstream>

namespace NEO {

class AUBFixture : public CommandQueueHwFixture {
  public:
    void SetUp(const HardwareInfo *hardwareInfo) {
        const HardwareInfo &hwInfo = hardwareInfo ? *hardwareInfo : *platformDevices[0];
        uint32_t deviceIndex = 0;

        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto engineType = getChosenEngineType(hwInfo);

        const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::stringstream strfilename;
        strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

        executionEnvironment = platformImpl->peekExecutionEnvironment();
        executionEnvironment->setHwInfo(&hwInfo);
        if (testMode == TestMode::AubTestsWithTbx) {
            this->csr = TbxCommandStreamReceiver::create(strfilename.str(), true, *executionEnvironment);
        } else {
            this->csr = AUBCommandStreamReceiver::create(strfilename.str(), true, *executionEnvironment);
        }

        executionEnvironment->commandStreamReceivers.resize(deviceIndex + 1);

        device.reset(MockDevice::create<MockDevice>(executionEnvironment, deviceIndex));
        device->resetCommandStreamReceiver(this->csr);

        CommandQueueHwFixture::SetUp(AUBFixture::device.get(), cl_command_queue_properties(0));
    }
    void TearDown() override {
        CommandQueueHwFixture::TearDown();
    }

    GraphicsAllocation *createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size);

    template <typename FamilyType>
    CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr() const {
        return static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            tbxCsr->expectMemoryEqual(gfxAddress, srcAddress, length);
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectNotEqualMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            tbxCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length);
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryNotEqual(gfxAddress, srcAddress, length);
        }
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
}; // namespace NEO

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
} // namespace NEO
