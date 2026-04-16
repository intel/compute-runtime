/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_nvlp.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

#include "per_product_test_definitions.h"

using namespace NEO;

template <typename FamilyType>
struct CopyEnginesNvlFixture : public CopyEngineXeHPAndLater<1, false> {

    bool compressionSupported() const override {
        auto &ftrTable = MulticontextOclAubFixture::rootDevice->getHardwareInfo().featureTable;
        return (ftrTable.flags.ftrFlatPhysCCS);
    }
};

using SingleTileSystemMemNvlCoreTests = CopyEnginesNvlFixture<Xe3pCoreFamily>;

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolve) {
    givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

NVLPTEST_F(SingleTileSystemMemNvlCoreTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}
