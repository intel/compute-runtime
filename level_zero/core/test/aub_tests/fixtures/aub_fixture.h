/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/core/test/unit_tests/mock.h"
#include <level_zero/zes_api.h>

#include "test_mode.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {
class CommandStreamReceiver;
class MockDevice;
class ExecutionEnvironment;
class MemoryManager;
struct HardwareInfo;
} // namespace NEO
namespace L0 {
namespace ult {
template <typename Type>
struct Mock;
template <typename Type>
struct WhiteBox;
} // namespace ult

struct ContextImp;
struct DriverHandleImp;
struct CommandQueue;
struct CommandListImp;
struct Device;

class AUBFixtureL0 {
  public:
    AUBFixtureL0();
    virtual ~AUBFixtureL0();
    void setUp();
    void setUp(const NEO::HardwareInfo *hardwareInfo, bool debuggingEnabled);
    void tearDown();

    template <typename FamilyType>
    NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr() const {
        return static_cast<NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(csr);
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (NEO::testMode == NEO::TestMode::aubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<NEO::CommandStreamReceiverWithAUBDump<NEO::TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectNotEqualMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>();

        if (NEO::testMode == NEO::TestMode::aubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<NEO::CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<NEO::CommandStreamReceiverWithAUBDump<NEO::TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryNotEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        NEO::AUBCommandStreamReceiverHw<FamilyType> *aubCsr = static_cast<NEO::AUBCommandStreamReceiverHw<FamilyType> *>(csr);
        if (NEO::testMode == NEO::TestMode::aubTestsWithTbx) {
            aubCsr = static_cast<NEO::AUBCommandStreamReceiverHw<FamilyType> *>(static_cast<NEO::CommandStreamReceiverWithAUBDump<NEO::TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get());
        }

        if (aubCsr) {
            aubCsr->expectMMIO(mmioRegister, expectedValue);
        }
    }

    static ze_module_handle_t createModuleFromFile(const std::string &fileName, ze_context_handle_t context, ze_device_handle_t device, const std::string &buildFlags, bool useSharedFile = false);

    std::string aubFileName;
    std::unique_ptr<VariableBackup<NEO::UltHwConfig>> backupUltConfig;

    const uint32_t rootDeviceIndex = 0;
    NEO::ExecutionEnvironment *executionEnvironment;
    NEO::MemoryManager *memoryManager = nullptr;
    NEO::MockDevice *neoDevice = nullptr;

    std::unique_ptr<ult::Mock<DriverHandleImp>> driverHandle;
    std::unique_ptr<ult::WhiteBox<L0::CommandListImp>> commandList;

    Device *device = nullptr;
    ContextImp *context = nullptr;
    CommandQueue *pCmdq = nullptr;

    NEO::CommandStreamReceiver *csr = nullptr;
};

} // namespace L0
