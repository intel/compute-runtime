/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

#include <memory>
#include <vector>

namespace NEO {
class MockDevice;

struct MulticontextAubFixture {
    enum class EnabledCommandStreamers {
        Single, // default only
        Dual,   // RCS + CCS0
        All,    // RCS + CCS0-3
    };

    void SetUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression); // NOLINT(readability-identifier-naming)
    void TearDown();                                                                                             // NOLINT(readability-identifier-naming)

    template <typename FamilyType>
    CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr(uint32_t tile, uint32_t engine) {
        using CsrWithAubDump = CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>;
        using SimulatedCsr = CommandStreamReceiverSimulatedCommonHw<FamilyType>;
        SimulatedCsr *simulatedCsr = nullptr;

        if (testMode == TestMode::AubTestsWithTbx) {
            auto csrWithAubDump = static_cast<CsrWithAubDump *>(&commandQueues[tile][engine]->getGpgpuCommandStreamReceiver());
            simulatedCsr = static_cast<SimulatedCsr *>(csrWithAubDump);
        } else {
            simulatedCsr = static_cast<SimulatedCsr *>(&commandQueues[tile][engine]->getGpgpuCommandStreamReceiver());
        }

        return simulatedCsr;
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length, uint32_t tile, uint32_t engine) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>(tile, engine);

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csrSimulated)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length, uint32_t tile, uint32_t engine) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>(tile, engine);

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryNotEqual(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csrSimulated)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryNotEqual(gfxAddress, srcAddress, length);
        }
    }

    template <typename FamilyType>
    void expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length, uint32_t tile, uint32_t engine) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>(tile, engine);

        if (testMode == TestMode::AubTestsWithTbx) {
            auto tbxCsr = csrSimulated;
            EXPECT_TRUE(tbxCsr->expectMemoryCompressed(gfxAddress, srcAddress, length));
            csrSimulated = static_cast<CommandStreamReceiverSimulatedCommonHw<FamilyType> *>(
                static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csrSimulated)->aubCSR.get());
        }

        if (csrSimulated) {
            csrSimulated->expectMemoryCompressed(gfxAddress, srcAddress, length);
        }
    }

    void overridePlatformConfigForAllEnginesSupport(HardwareInfo &localHwInfo);
    void adjustPlatformOverride(HardwareInfo &localHwInfo, bool &setupCalled);
    DebugManagerStateRestore restore;

    const uint32_t rootDeviceIndex = 0u;
    uint32_t numberOfEnabledTiles = 0;
    std::vector<ClDevice *> tileDevices;
    ClDevice *rootDevice = nullptr;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockContext> multiTileDefaultContext;
    std::vector<std::vector<std::unique_ptr<CommandQueue>>> commandQueues;
};
} // namespace NEO
