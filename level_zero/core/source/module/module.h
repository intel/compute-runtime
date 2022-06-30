/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <set>
#include <vector>

struct _ze_module_handle_t {};

namespace L0 {
struct Device;
struct ModuleBuildLog;
struct KernelImmutableData;

enum class ModuleType {
    Builtin,
    User
};

struct Module : _ze_module_handle_t {

    static Module *create(Device *device, const ze_module_desc_t *desc, ModuleBuildLog *moduleBuildLog, ModuleType type);

    virtual ~Module() = default;

    virtual Device *getDevice() const = 0;

    virtual ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                     ze_kernel_handle_t *phFunction) = 0;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) = 0;
    virtual ze_result_t getFunctionPointer(const char *pKernelName, void **pfnFunction) = 0;
    virtual ze_result_t getGlobalPointer(const char *pGlobalName, size_t *pSize, void **pPtr) = 0;
    virtual ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) = 0;
    virtual ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) = 0;
    virtual ze_result_t getProperties(ze_module_properties_t *pModuleProperties) = 0;
    virtual ze_result_t performDynamicLink(uint32_t numModules,
                                           ze_module_handle_t *phModules,
                                           ze_module_build_log_handle_t *phLinkLog) = 0;

    virtual const KernelImmutableData *getKernelImmutableData(const char *functionName) const = 0;
    virtual const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const = 0;
    virtual uint32_t getMaxGroupSize() const = 0;
    virtual bool isDebugEnabled() const = 0;
    virtual bool shouldAllocatePrivateMemoryPerDispatch() const = 0;
    virtual uint32_t getProfileFlags() const = 0;
    virtual void checkIfPrivateMemoryPerDispatchIsNeeded() = 0;

    Module() = default;
    Module(const Module &) = delete;
    Module(Module &&) = delete;
    Module &operator=(const Module &) = delete;
    Module &operator=(Module &&) = delete;

    static Module *fromHandle(ze_module_handle_t handle) { return static_cast<Module *>(handle); }

    inline ze_module_handle_t toHandle() { return this; }
};

} // namespace L0
