/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <sstream>

namespace NEO {

class AUBFixture : public CommandQueueHwFixture {
  public:
    void setUp(const HardwareInfo *hardwareInfo) {
        const HardwareInfo &hwInfo = hardwareInfo ? *hardwareInfo : *defaultHwInfo;

        backupUltConfig = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);

        executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1u);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MockMemoryOperationsHandler>();
        executionEnvironment->calculateMaxOsContextCount();

        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        auto engineType = getChosenEngineType(hwInfo);

        const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        std::stringstream strfilename;
        strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
        strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << gfxCoreHelper.getCsTraits(engineType).name;

        aubFileName = strfilename.str();
        ultHwConfig.aubTestName = aubFileName.c_str();

        auto pDevice = MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex);
        pDevice->disableSecondaryEngines = true;
        initPlatform({pDevice});
        device = static_cast<MockClDevice *>(platform(pDevice->getExecutionEnvironment())->getClDevice(0));

        this->csr = pDevice->getDefaultEngine().commandStreamReceiver;

        CommandQueueHwFixture::setUp(AUBFixture::device, cl_command_queue_properties(0));
    }
    void tearDown() {
        CommandQueueHwFixture::tearDown();
    }

    GraphicsAllocation *createHostPtrAllocationFromSvmPtr(void *svmPtr, size_t size);
    GraphicsAllocation *createResidentAllocationAndStoreItInCsr(const void *address, size_t size);

    template <typename FamilyType>
    CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr() const {
        return static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (testMode == TestMode::aubTestsWithTbx) {
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
        if (testMode == TestMode::aubTestsWithTbx) {
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

        if (testMode == TestMode::aubTestsWithTbx) {
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

        if (testMode == TestMode::aubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryCompressed(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryCompressed(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void pollForCompletion() {
        getSimulatedCsr<FamilyType>()->pollForCompletion();
    }

    static void *getGpuPointer(GraphicsAllocation *allocation) {
        return reinterpret_cast<void *>(allocation->getGpuAddress());
    }

    static void *getGpuPointer(GraphicsAllocation *allocation, size_t offset) {
        return reinterpret_cast<void *>(allocation->getGpuAddress() + offset);
    }

    const uint32_t rootDeviceIndex = 0;
    CommandStreamReceiver *csr = nullptr;
    volatile uint32_t *pTagMemory = nullptr;
    MockClDevice *device;

    ExecutionEnvironment *executionEnvironment;

  private:
    using CommandQueueHwFixture::setUp;
    std::string aubFileName;
    std::unique_ptr<VariableBackup<UltHwConfig>> backupUltConfig;

}; // namespace NEO

template <typename KernelFixture>
struct KernelAUBFixture : public AUBFixture,
                          public KernelFixture {
    void setUp() {
        AUBFixture::setUp(nullptr);
        KernelFixture::setUp(device, context);
    }

    void tearDown() {
        KernelFixture::tearDown();
        AUBFixture::tearDown();
    }
};
} // namespace NEO
