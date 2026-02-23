/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/implicit_args_base.h"
#include "shared/test/unit_test/kernel/implicit_args_layout_test_helper.h"

#include "implicit_args.h"

OffsetMap getImplicitArgsV2ExpectedOffsets() {
    return {
        {"structSize", offsetof(NEO::ImplicitArgsV2, header) + offsetof(NEO::ImplicitArgsHeader, structSize)},
        {"structVersion", offsetof(NEO::ImplicitArgsV2, header) + offsetof(NEO::ImplicitArgsHeader, structVersion)},
        {"numWorkDim", offsetof(NEO::ImplicitArgsV2, numWorkDim)},
        {"padding0", offsetof(NEO::ImplicitArgsV2, padding0)},
        {"localSizeX", offsetof(NEO::ImplicitArgsV2, localSizeX)},
        {"localSizeY", offsetof(NEO::ImplicitArgsV2, localSizeY)},
        {"localSizeZ", offsetof(NEO::ImplicitArgsV2, localSizeZ)},
        {"globalSizeX", offsetof(NEO::ImplicitArgsV2, globalSizeX)},
        {"globalSizeY", offsetof(NEO::ImplicitArgsV2, globalSizeY)},
        {"globalSizeZ", offsetof(NEO::ImplicitArgsV2, globalSizeZ)},
        {"printfBufferPtr", offsetof(NEO::ImplicitArgsV2, printfBufferPtr)},
        {"globalOffsetX", offsetof(NEO::ImplicitArgsV2, globalOffsetX)},
        {"globalOffsetY", offsetof(NEO::ImplicitArgsV2, globalOffsetY)},
        {"globalOffsetZ", offsetof(NEO::ImplicitArgsV2, globalOffsetZ)},
        {"localIdTablePtr", offsetof(NEO::ImplicitArgsV2, localIdTablePtr)},
        {"groupCountX", offsetof(NEO::ImplicitArgsV2, groupCountX)},
        {"groupCountY", offsetof(NEO::ImplicitArgsV2, groupCountY)},
        {"groupCountZ", offsetof(NEO::ImplicitArgsV2, groupCountZ)},
        {"padding1", offsetof(NEO::ImplicitArgsV2, padding1)},
        {"rtGlobalBufferPtr", offsetof(NEO::ImplicitArgsV2, rtGlobalBufferPtr)},
        {"assertBufferPtr", offsetof(NEO::ImplicitArgsV2, assertBufferPtr)},
        {"syncBufferPtr", offsetof(NEO::ImplicitArgsV2, syncBufferPtr)},
        {"enqueuedLocalSizeX", offsetof(NEO::ImplicitArgsV2, enqueuedLocalSizeX)},
        {"enqueuedLocalSizeY", offsetof(NEO::ImplicitArgsV2, enqueuedLocalSizeY)},
        {"enqueuedLocalSizeZ", offsetof(NEO::ImplicitArgsV2, enqueuedLocalSizeZ)},
        {"reserved", offsetof(NEO::ImplicitArgsV2, reserved)},
    };
}
