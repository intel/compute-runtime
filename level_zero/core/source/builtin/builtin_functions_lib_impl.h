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
enum class Group : uint32_t;
}
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

    Kernel *getFunction(BufferBuiltIn func) override;
    Kernel *getImageFunction(ImageBuiltIn func) override;
    void initBuiltinKernel(BufferBuiltIn builtId) override;
    void initBuiltinImageKernel(ImageBuiltIn func) override;
    void ensureInitCompletion() override;
    void ensureInitCompletionImpl();
    MOCKABLE_VIRTUAL std::unique_ptr<BuiltInKernelLibImpl::BuiltInKernelData> loadBuiltIn(NEO::BuiltIn::Group builtInGroup, const char *kernelName);

    static bool initBuiltinsAsyncEnabled(Device *device);

  protected:
    std::vector<std::unique_ptr<Module>> modules = {};
    std::unique_ptr<BuiltInKernelData> builtins[static_cast<uint32_t>(BufferBuiltIn::count)];
    std::unique_ptr<BuiltInKernelData> imageBuiltins[static_cast<uint32_t>(ImageBuiltIn::count)];
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
