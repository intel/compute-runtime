/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/program/program_info.h"
#include "shared/source/utilities/const_stringref.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/module/module.h"

#include "igfxfmid.h"

#include <memory>
#include <string>

namespace L0 {

struct ModuleTranslationUnit {
    ModuleTranslationUnit(L0::Device *device);
    virtual ~ModuleTranslationUnit();
    bool buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                        const ze_module_constants_t *pConstants);
    bool createFromNativeBinary(const char *input, size_t inputSize);
    MOCKABLE_VIRTUAL bool processUnpackedBinary();
    void updateBuildLog(const std::string &newLogEntry);
    void processDebugData();
    L0::Device *device = nullptr;

    NEO::GraphicsAllocation *globalConstBuffer = nullptr;
    NEO::GraphicsAllocation *globalVarBuffer = nullptr;
    NEO::ProgramInfo programInfo;

    std::string options;

    std::string buildLog;

    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize = 0U;

    std::unique_ptr<char[]> unpackedDeviceBinary;
    size_t unpackedDeviceBinarySize = 0U;

    std::unique_ptr<char[]> packedDeviceBinary;
    size_t packedDeviceBinarySize = 0U;

    std::unique_ptr<char[]> debugData;
    size_t debugDataSize = 0U;

    NEO::specConstValuesMap specConstantsValues;
};

struct ModuleImp : public Module {
    ModuleImp() = delete;

    ModuleImp(Device *device, ModuleBuildLog *moduleBuildLog);

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

    ze_result_t performDynamicLink(uint32_t numModules,
                                   ze_module_handle_t *phModules,
                                   ze_module_build_log_handle_t *phLinkLog) override;

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

    ModuleTranslationUnit *getTranslationUnit() {
        return this->translationUnit.get();
    }

    const std::set<NEO::GraphicsAllocation *> &getImportedSymbolAllocations() const override { return importedSymbolAllocations; }

  protected:
    void copyPatchedSegments(const NEO::Linker::PatchableSegments &isaSegmentsForPatching);
    void verifyDebugCapabilities();
    Device *device = nullptr;
    PRODUCT_FAMILY productFamily{};
    std::unique_ptr<ModuleTranslationUnit> translationUnit;
    ModuleBuildLog *moduleBuildLog = nullptr;
    NEO::GraphicsAllocation *exportedFunctionsSurface = nullptr;
    uint32_t maxGroupSize = 0U;
    std::vector<std::unique_ptr<KernelImmutableData>> kernelImmDatas;
    NEO::Linker::RelocatedSymbolsMap symbols;
    bool debugEnabled = false;
    bool isFullyLinked = false;
    NEO::Linker::UnresolvedExternals unresolvedExternalsInfo{};
    std::set<NEO::GraphicsAllocation *> importedSymbolAllocations{};
};

bool moveBuildOption(std::string &dstOptionsSet, std::string &srcOptionSet, NEO::ConstStringRef dstOptionName, NEO::ConstStringRef srcOptionName);

} // namespace L0
