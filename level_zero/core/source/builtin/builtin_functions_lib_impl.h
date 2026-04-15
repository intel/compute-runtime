/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/module/module.h"

#include <atomic>
#include <vector>

namespace NEO {
namespace BuiltIn {
enum class BaseKernel : uint32_t;
struct AddressingMode;
} // namespace BuiltIn
class BuiltIns;
} // namespace NEO

namespace L0 {
struct Kernel;
struct Device;

struct BuiltInKernelLibImpl : BuiltInKernelLib, NEO::NonCopyableClass {
    struct BuiltInKernelData;
    BuiltInKernelLibImpl(Device *device, NEO::BuiltIns *builtInsLib);
    ~BuiltInKernelLibImpl() override {
        this->ensureInitCompletionImpl();
    }

    Kernel *getFunction(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override;
    Kernel *getImageFunction(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override;
    void initBuiltinKernel(BufferBuiltIn builtId, const NEO::BuiltIn::AddressingMode &mode) override;
    void initBuiltinImageKernel(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override;
    void ensureInitCompletion() override;
    void ensureInitCompletionImpl();
    MOCKABLE_VIRTUAL std::unique_ptr<BuiltInKernelLibImpl::BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::BaseKernel baseKernel, const NEO::BuiltIn::AddressingMode &mode, const char *kernelName);

    static bool initBuiltinsAsyncEnabled(Device *device);

  protected:
    static constexpr uint32_t maxBufferCacheSize = static_cast<uint32_t>(BufferBuiltIn::count) * NEO::BuiltIn::addressingModeCount;
    static constexpr uint32_t maxImageCacheSize = static_cast<uint32_t>(ImageBuiltIn::count) * NEO::BuiltIn::addressingModeCount;

    uint32_t toBufferCacheIndex(BufferBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) const {
        return static_cast<uint32_t>(func) * NEO::BuiltIn::addressingModeCount + mode.toIndex();
    }
    uint32_t toImageCacheIndex(ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) const {
        return static_cast<uint32_t>(func) * NEO::BuiltIn::addressingModeCount + mode.toIndex();
    }

    std::vector<std::unique_ptr<Module>> modules = {};
    std::unique_ptr<BuiltInKernelData> builtins[maxBufferCacheSize];
    std::unique_ptr<BuiltInKernelData> imageBuiltins[maxImageCacheSize];
    Device *device;
    NEO::BuiltIns *builtInsLib;

    bool initAsyncComplete = true;
    std::atomic_bool initAsync = false;
};
struct BuiltInKernelLibImpl::BuiltInKernelData : NEO::NonCopyableClass {
    MOCKABLE_VIRTUAL ~BuiltInKernelData();
    BuiltInKernelData();
    BuiltInKernelData(Module *module, std::unique_ptr<L0::Kernel> &&ker);

    Module *module = nullptr;
    std::unique_ptr<Kernel> func;
};

static_assert(NEO::NonCopyable<BuiltInKernelLibImpl>);
static_assert(NEO::NonCopyable<BuiltInKernelLibImpl::BuiltInKernelData>);
} // namespace L0
