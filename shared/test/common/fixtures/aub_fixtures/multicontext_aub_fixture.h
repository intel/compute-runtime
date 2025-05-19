/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/tests_configuration.h"

#include "gtest/gtest.h"

#include <memory>
#include <vector>

namespace NEO {
class Device;
class MockDevice;
class SVMAllocsManager;

struct MulticontextAubFixture {
    enum class EnabledCommandStreamers {
        single, // default only
        dual,   // RCS + CCS0
        all,    // RCS + CCS0-3
    };

    MulticontextAubFixture() = default;
    virtual ~MulticontextAubFixture() = default;

    void setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression);

    virtual void createDevices(const HardwareInfo &hwInfo, uint32_t numTiles) = 0;

    void tearDown() {}

    virtual CommandStreamReceiver *getGpgpuCsr(uint32_t tile, uint32_t engine) = 0;

    bool isMemoryCompressed(CommandStreamReceiver *csr, void *gfxAddress);

    template <typename FamilyType>
    CommandStreamReceiverSimulatedCommonHw<FamilyType> *getSimulatedCsr(uint32_t tile, uint32_t engine) {
        using CsrWithAubDump = CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>;
        using SimulatedCsr = CommandStreamReceiverSimulatedCommonHw<FamilyType>;
        SimulatedCsr *simulatedCsr = nullptr;

        auto csr = getGpgpuCsr(tile, engine);

        if (testMode == TestMode::aubTestsWithTbx) {
            auto csrWithAubDump = static_cast<CsrWithAubDump *>(csr);
            simulatedCsr = static_cast<SimulatedCsr *>(csrWithAubDump);
        } else {
            simulatedCsr = static_cast<SimulatedCsr *>(csr);
        }

        return simulatedCsr;
    }

    template <typename FamilyType>
    void expectMemory(void *gfxAddress, const void *srcAddress, size_t length, uint32_t tile, uint32_t engine) {
        CommandStreamReceiverSimulatedCommonHw<FamilyType> *csrSimulated = getSimulatedCsr<FamilyType>(tile, engine);
        // expectMemory should not be used for compressed memory
        ASSERT_FALSE(isMemoryCompressed(csrSimulated, gfxAddress));

        if (testMode == TestMode::aubTestsWithTbx) {
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

        if (testMode == TestMode::aubTestsWithTbx) {
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

        if (testMode == TestMode::aubTestsWithTbx) {
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

    SVMAllocsManager *svmAllocsManager = nullptr;
    const uint32_t rootDeviceIndex = 0u;
    uint32_t numberOfEnabledTiles = 0;
    bool isCcs1Supported = false;
    bool isRenderEngineSupported = true;
    bool isFirstEngineBcs = false;
    bool skipped = false;
    DispatchMode dispatchMode = DispatchMode::batchedDispatch;
};
} // namespace NEO
