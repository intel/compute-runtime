/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/kernel.h"
#include "level_zero/core/source/module_build_log.h"
#include <level_zero/ze_api.h>

#include <vector>

struct _ze_module_handle_t {};

namespace L0 {
struct Device;

struct Module : _ze_module_handle_t {
    static Module *create(Device *device, const ze_module_desc_t *desc, NEO::Device *neoDevice,
                          ModuleBuildLog *moduleBuildLog);

    virtual ~Module() = default;

    virtual Device *getDevice() const = 0;

    virtual ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                     ze_kernel_handle_t *phFunction) = 0;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) = 0;
    virtual ze_result_t getFunctionPointer(const char *pKernelName, void **pfnFunction) = 0;
    virtual ze_result_t getGlobalPointer(const char *pGlobalName, void **pPtr) = 0;
    virtual ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) = 0;
    virtual ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) = 0;

    virtual const KernelImmutableData *getKernelImmutableData(const char *functionName) const = 0;
    virtual const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const = 0;
    virtual uint32_t getMaxGroupSize() const = 0;
    virtual bool isDebugEnabled() const = 0;

    Module() = default;
    Module(const Module &) = delete;
    Module(Module &&) = delete;
    Module &operator=(const Module &) = delete;
    Module &operator=(Module &&) = delete;

    static Module *fromHandle(ze_module_handle_t handle) { return static_cast<Module *>(handle); }

    inline ze_module_handle_t toHandle() { return this; }
};

} // namespace L0
