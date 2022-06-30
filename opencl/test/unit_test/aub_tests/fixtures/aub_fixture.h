/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <sstream>

namespace NEO {

class AUBFixture : public CommandQueueHwFixture {
  public:
    static CommandStreamReceiver *prepareComputeEngine(MockDevice &device, const std::string &filename) {
        CommandStreamReceiver *pCommandStreamReceiver = nullptr;
        if (testMode == TestMode::AubTestsWithTbx) {
            pCommandStreamReceiver = TbxCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
        } else {
            pCommandStreamReceiver = AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
        }
        device.resetCommandStreamReceiver(pCommandStreamReceiver);
        return pCommandStreamReceiver;
    }
    static void prepareCopyEngines(MockDevice &device, const std::string &filename) {
        for (auto i = 0u; i < device.allEngines.size(); i++) {
            if (EngineHelpers::isBcs(device.allEngines[i].getEngineType())) {
                CommandStreamReceiver *pBcsCommandStreamReceiver = nullptr;
                if (testMode == TestMode::AubTestsWithTbx) {
                    pBcsCommandStreamReceiver = TbxCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
                } else {
                    pBcsCommandStreamReceiver = AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
                }
                device.resetCommandStreamReceiver(pBcsCommandStreamReceiver, i);
            }
        }
    }

    void SetUp(const HardwareInfo *hardwareInfo) {
        const HardwareInfo &hwInfo = hardwareInfo ? *hardwareInfo : *defaultHwInfo;

        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto engineType = getChosenEngineType(hwInfo);

        const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::stringstream strfilename;
        strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
        strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

        executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MockMemoryOperationsHandler>();

        auto pDevice = MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex);
        device = std::make_unique<MockClDevice>(pDevice);

        this->csr = prepareComputeEngine(*pDevice, strfilename.str());

        prepareCopyEngines(*pDevice, strfilename.str());

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
            EXPECT_TRUE(tbxCsr->expectMemoryEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void writeMMIO(uint32_t offset, uint32_t value) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();
        if (csrSimulated) {
            csrSimulated->writeMMIO(offset, value);
        }
    }

    template <typename FamilyType>
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        CommandStreamReceiver *csrtemp = csr;
        if (testMode == TestMode::AubTestsWithTbx) {
            csrtemp = static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get();
        }

        if (csrtemp) {
            // Write our pseudo-op to the AUB file
            auto aubCsr = static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csrtemp);
            aubCsr->expectMMIO(mmioRegister, expectedValue);
        }
    }

    template <typename FamilyType>
    void expectNotEqualMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryNotEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectCompressedMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryCompressed(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryCompressed(gfxAddress, srcAddress, length);
        }
    }

    static void *getGpuPointer(GraphicsAllocation *allocation) {
        return reinterpret_cast<void *>(allocation->getGpuAddress());
    }

    const uint32_t rootDeviceIndex = 0;
    CommandStreamReceiver *csr = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    std::unique_ptr<MockClDevice> device;

    ExecutionEnvironment *executionEnvironment;

  private:
    using CommandQueueHwFixture::SetUp;
}; // namespace NEO

template <typename KernelFixture>
struct KernelAUBFixture : public AUBFixture,
                          public KernelFixture {
    void SetUp() override {
        AUBFixture::SetUp(nullptr);
        KernelFixture::SetUp(device.get(), context);
    }

    void TearDown() override {
        KernelFixture::TearDown();
        AUBFixture::TearDown();
    }
};
} // namespace NEO
