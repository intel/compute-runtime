/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/utilities/const_stringref.h"

#include "level_zero/core/source/device.h"
#include "level_zero/core/source/module.h"

#include "igfxfmid.h"

#include <memory>
#include <string>

namespace L0 {

struct ModuleTranslationUnit;

struct ModuleImp : public Module {
    ModuleImp(Device *device, NEO::Device *neoDevice, ModuleBuildLog *moduleBuildLog);

    ~ModuleImp() override;

    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t createKernel(const ze_kernel_desc_t *desc,
                             ze_kernel_handle_t *phFunction) override;

    ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) override;

    ze_result_t getFunctionPointer(const char *pFunctionName, void **pfnFunction) override;

    ze_result_t getGlobalPointer(const char *pGlobalName, void **pPtr) override;

    ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) override;

    ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) override;

    const KernelImmutableData *getKernelImmutableData(const char *functionName) const override;

    const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const override { return kernelImmDatas; }

    uint32_t getMaxGroupSize() const override { return maxGroupSize; }

    void createBuildOptions(const char *pBuildFlags, std::string &buildOptions, std::string &internalBuildOptions);
    void createBuildExtraOptions(std::string &buildOptions, std::string &internalBuildOptions);
    void updateBuildLog(NEO::Device *neoDevice);

    Device *getDevice() const override { return device; }

    bool linkBinary();

    bool initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice);

    bool isDebugEnabled() const override;

  protected:
    ModuleImp() = default;

    Device *device = nullptr;
    PRODUCT_FAMILY productFamily{};
    ModuleTranslationUnit *translationUnit = nullptr;
    ModuleBuildLog *moduleBuildLog = nullptr;
    NEO::GraphicsAllocation *exportedFunctionsSurface = nullptr;
    uint32_t maxGroupSize = 0U;
    std::vector<std::unique_ptr<KernelImmutableData>> kernelImmDatas;
    NEO::Linker::RelocatedSymbolsMap symbols;
    bool debugEnabled = false;
};

bool moveBuildOption(std::string &dstOptionsSet, std::string &srcOptionSet, ConstStringRef dstOptionName, ConstStringRef srcOptionName);

} // namespace L0
