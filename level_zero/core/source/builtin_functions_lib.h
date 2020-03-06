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
    CopyBufferToBufferSide,
    CopyBufferToBufferMiddle,
    CopyImageRegion,
    FillBufferImmediate,
    FillBufferSSHOffset,
    CopyBufferRectBytes2d,
    CopyBufferRectBytes3d,
    CopyBufferToImage3dBytes,
    CopyBufferToImage3d2Bytes,
    CopyBufferToImage3d4Bytes,
    CopyBufferToImage3d8Bytes,
    CopyBufferToImage3d16Bytes,
    CopyImage3dToBufferBytes,
    CopyImage3dToBuffer2Bytes,
    CopyImage3dToBuffer4Bytes,
    CopyImage3dToBuffer8Bytes,
    CopyImage3dToBuffer16Bytes,
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
