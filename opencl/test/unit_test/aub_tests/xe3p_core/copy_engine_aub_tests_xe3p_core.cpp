/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

template <uint32_t numTiles, typename FamilyType, bool useLocalMemory>
struct CopyEnginesXe3pCoreFixture : public CopyEngineXeHPAndLater<numTiles, useLocalMemory> {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    void SetUp() override {
        if (defaultHwInfo->featureTable.ftrBcsInfo.count() == 9) {
            debugManager.flags.BlitterEnableMaskOverride.set(6);
        }

        CopyEngineXeHPAndLater<numTiles, useLocalMemory>::SetUp();
    }
};

using SingleTileXe3pCoreTests = CopyEnginesXe3pCoreFixture<1, Xe3pCoreFamily, true>;

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolve) {
    givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}

using SingleTileSystemMemoryXe3pCoreTests = CopyEnginesXe3pCoreFixture<1, Xe3pCoreFamily, false>;

XE3P_CORETEST_F(SingleTileSystemMemoryXe3pCoreTests, givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE3P_CORETEST_F(SingleTileXe3pCoreTests, givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}
