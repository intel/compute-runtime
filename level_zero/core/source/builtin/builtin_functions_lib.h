/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <memory>

namespace NEO {
class BuiltIns;
} // namespace NEO

namespace L0 {
struct Device;
struct Kernel;

enum class Builtin : uint32_t {
    CopyBufferBytes = 0u,
    CopyBufferRectBytes2d,
    CopyBufferRectBytes3d,
    CopyBufferToBufferMiddle,
    CopyBufferToBufferSide,
    CopyBufferToImage3d16Bytes,
    CopyBufferToImage3d2Bytes,
    CopyBufferToImage3d4Bytes,
    CopyBufferToImage3d8Bytes,
    CopyBufferToImage3dBytes,
    CopyImage3dToBuffer16Bytes,
    CopyImage3dToBuffer2Bytes,
    CopyImage3dToBuffer4Bytes,
    CopyImage3dToBuffer8Bytes,
    CopyImage3dToBufferBytes,
    CopyImageRegion,
    FillBufferImmediate,
    FillBufferSSHOffset,
    COUNT
};

struct BuiltinFunctionsLib {
    virtual ~BuiltinFunctionsLib() = default;
    static std::unique_ptr<BuiltinFunctionsLib> create(Device *device,
                                                       NEO::BuiltIns *builtins);

    virtual Kernel *getFunction(Builtin func) = 0;
    virtual void initFunctions() = 0;
    virtual Kernel *getPageFaultFunction() = 0;
    virtual void initPageFaultFunction() = 0;

  protected:
    BuiltinFunctionsLib() = default;
};

} // namespace L0
