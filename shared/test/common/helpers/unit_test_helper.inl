/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "gtest/gtest.h"
#include "unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getTdCtlRegisterOffset() {
    return 0xe400;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::useFullRowForLocalIdsGeneration = false;

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::additionalMiFlushDwRequired = false;

template <typename GfxFamily>
inline uint64_t UnitTestHelper<GfxFamily>::getPipeControlPostSyncAddress(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    uint64_t gpuAddress = pipeControl.getAddress();
    uint64_t gpuAddressHigh = pipeControl.getAddressHigh();

    return (gpuAddressHigh << 32) | gpuAddress;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    HardwareParse hwParse;
    hwParse.parseCommands<GfxFamily>(csr.getCS(0), 0);
    auto itorCmd = reverseFind<STATE_BASE_ADDRESS *>(hwParse.cmdList.rbegin(), hwParse.cmdList.rend());
    EXPECT_NE(hwParse.cmdList.rend(), itorCmd);
    auto sba = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);
    EXPECT_NE(nullptr, sba);

    auto mocs = sba->getStatelessDataPortAccessMemoryObjectControlState();

    EXPECT_EQ(expectedMocs, mocs);
}

template <typename GfxFamily>
bool UnitTestHelperWithHeap<GfxFamily>::getDisableFusionStateFromFrontEndCommand(const typename GfxFamily::FrontEndStateCommand &feCmd) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelperWithHeap<GfxFamily>::getComputeDispatchAllWalkerFromFrontEndCommand(const typename GfxFamily::FrontEndStateCommand &feCmd) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::expectNullDsh(const DeviceInfo &deviceInfo) {
    if constexpr (GfxFamily::supportsSampler) {
        return !deviceInfo.imageSupport;
    }
    return true;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getInlineDataSize(bool isHeaplessEnabled) {
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;
    return DefaultWalkerType::getInlineDataSize();
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::findStateCacheFlushPipeControl(CommandStreamReceiver &csr, LinearStream &csrStream) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    HardwareParse hwParserCsr;
    hwParserCsr.parsePipeControl = true;
    hwParserCsr.parseCommands<GfxFamily>(csrStream, 0);
    hwParserCsr.findHardwareCommands<GfxFamily>();

    bool stateCacheFlushFound = false;
    auto itorPipeControl = hwParserCsr.pipeControlList.begin();
    while (itorPipeControl != hwParserCsr.pipeControlList.end()) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*itorPipeControl);

        if (pipeControl->getRenderTargetCacheFlushEnable() &&
            pipeControl->getStateCacheInvalidationEnable() &&
            pipeControl->getTextureCacheInvalidationEnable() &&
            ((csr.isTlbFlushRequiredForStateCacheFlush() && pipeControl->getTlbInvalidate()) || (!csr.isTlbFlushRequiredForStateCacheFlush() && !pipeControl->getTlbInvalidate()))) {
            stateCacheFlushFound = true;
            break;
        }
        itorPipeControl++;
    }
    return stateCacheFlushFound;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getProgrammedGrfValue(CommandStreamReceiver &csr, LinearStream &linearStream) {
    return 0u;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getMiLoadRegisterImmProgrammedCmdsCount(bool debuggingEnabled) {
    return (debuggingEnabled ? 2u : 0u);
}

template <typename GfxFamily>
size_t UnitTestHelper<GfxFamily>::getWalkerSize(bool isHeaplessEnabled) {
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;
    return sizeof(DefaultWalkerType);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isHeaplessAllowed() {
    return false;
}

} // namespace NEO
