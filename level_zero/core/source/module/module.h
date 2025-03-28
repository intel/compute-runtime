/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <memory>
#include <vector>

struct _ze_module_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_module_handle_t>);

namespace NEO {
struct KernelDescriptor;
}

namespace L0 {
struct Device;
struct ModuleBuildLog;
struct KernelImmutableData;

enum class ModuleType {
    builtin,
    user
};

struct Module : _ze_module_handle_t, NEO::NonCopyableAndNonMovableClass {

    static Module *create(Device *device, const ze_module_desc_t *desc, ModuleBuildLog *moduleBuildLog, ModuleType type, ze_result_t *result);

    virtual ~Module() = default;

    virtual Device *getDevice() const = 0;

    virtual ze_result_t createKernel(const ze_kernel_desc_t *desc,
                                     ze_kernel_handle_t *kernelHandle) = 0;
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
    virtual ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *pInspectDesc,
                                       uint32_t numModules,
                                       ze_module_handle_t *phModules,
                                       ze_module_build_log_handle_t *phLog) = 0;

    virtual const KernelImmutableData *getKernelImmutableData(const char *kernelName) const = 0;
    virtual const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const = 0;
    virtual uint32_t getMaxGroupSize(const NEO::KernelDescriptor &kernelDescriptor) const = 0;
    virtual bool shouldAllocatePrivateMemoryPerDispatch() const = 0;
    virtual uint32_t getProfileFlags() const = 0;
    virtual void checkIfPrivateMemoryPerDispatchIsNeeded() = 0;
    virtual void populateZebinExtendedArgsMetadata() = 0;
    virtual void generateDefaultExtendedArgsMetadata() = 0;

    static Module *fromHandle(ze_module_handle_t handle) { return static_cast<Module *>(handle); }

    inline ze_module_handle_t toHandle() { return this; }
};

static_assert(NEO::NonCopyableAndNonMovable<Module>);

} // namespace L0
