/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

using namespace NEO;

template <uint32_t numTiles, typename FamilyType, bool useLocalMemory>
struct CopyEnginesLnlFixture : public CopyEngineXeHPAndLater<numTiles, useLocalMemory> {

    bool compressionSupported() const override {
        auto &ftrTable = MulticontextOclAubFixture::rootDevice->getHardwareInfo().featureTable;
        return (ftrTable.flags.ftrFlatPhysCCS && ftrTable.flags.ftrXe2Compression);
    }
};

using SingleTileSystemMemLnlCoreTests = CopyEnginesLnlFixture<1, Xe2HpgCoreFamily, false>;

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

LNLTEST_F(SingleTileSystemMemLnlCoreTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}
