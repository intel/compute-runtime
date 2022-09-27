/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/program/program_info.h"

#include "level_zero/core/source/module/module.h"

#include "igfxfmid.h"

#include <list>
#include <memory>
#include <string>

namespace NEO {
namespace Debug {
struct Segments;
}
} // namespace NEO
namespace L0 {

namespace BuildOptions {
extern NEO::ConstStringRef optDisable;
extern NEO::ConstStringRef optLevel;
extern NEO::ConstStringRef greaterThan4GbRequired;
extern NEO::ConstStringRef hasBufferOffsetArg;
extern NEO::ConstStringRef debugKernelEnable;
extern NEO::ConstStringRef profileFlags;
extern NEO::ConstStringRef optLargeRegisterFile;

} // namespace BuildOptions

struct ModuleTranslationUnit {
    ModuleTranslationUnit(L0::Device *device);
    virtual ~ModuleTranslationUnit();
    MOCKABLE_VIRTUAL ze_result_t buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                                const ze_module_constants_t *pConstants);
    MOCKABLE_VIRTUAL ze_result_t staticLinkSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes, const char *buildOptions, const char *internalBuildOptions,
                                                 std::vector<const ze_module_constants_t *> specConstants);
    MOCKABLE_VIRTUAL ze_result_t createFromNativeBinary(const char *input, size_t inputSize);
    MOCKABLE_VIRTUAL ze_result_t processUnpackedBinary();
    std::vector<uint8_t> generateElfFromSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes);
    bool processSpecConstantInfo(NEO::CompilerInterface *compilerInterface, const ze_module_constants_t *pConstants, const char *input, uint32_t inputSize);
    std::string generateCompilerOptions(const char *buildOptions, const char *internalBuildOptions);
    MOCKABLE_VIRTUAL ze_result_t compileGenBinary(NEO::TranslationInput inputArgs, bool staticLink);
    void updateBuildLog(const std::string &newLogEntry);
    void processDebugData();
    L0::Device *device = nullptr;

    NEO::GraphicsAllocation *globalConstBuffer = nullptr;
    NEO::GraphicsAllocation *globalVarBuffer = nullptr;
    NEO::ProgramInfo programInfo;

    std::string options;
    bool shouldSuppressRebuildWarning{false};

    std::string buildLog;

    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize = 0U;

    std::unique_ptr<char[]> unpackedDeviceBinary;
    size_t unpackedDeviceBinarySize = 0U;

    std::unique_ptr<char[]> packedDeviceBinary;
    size_t packedDeviceBinarySize = 0U;

    std::unique_ptr<char[]> debugData;
    size_t debugDataSize = 0U;
    std::vector<char *> alignedvIsas;

    NEO::specConstValuesMap specConstantsValues;
    bool isBuiltIn{false};
};

struct ModuleImp : public Module {
    ModuleImp() = delete;

    ModuleImp(Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type);

    ~ModuleImp() override;

    ze_result_t destroy() override;

    ze_result_t createKernel(const ze_kernel_desc_t *desc,
                             ze_kernel_handle_t *kernelHandle) override;

    ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) override;

    ze_result_t getFunctionPointer(const char *pFunctionName, void **pfnFunction) override;

    ze_result_t getGlobalPointer(const char *pGlobalName, size_t *pSize, void **pPtr) override;

    ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) override;

    ze_result_t getProperties(ze_module_properties_t *pModuleProperties) override;

    ze_result_t performDynamicLink(uint32_t numModules,
                                   ze_module_handle_t *phModules,
                                   ze_module_build_log_handle_t *phLinkLog) override;

    ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) override;

    const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override;

    const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const override { return kernelImmDatas; }

    uint32_t getMaxGroupSize() const override { return maxGroupSize; }

    void createBuildOptions(const char *pBuildFlags, std::string &buildOptions, std::string &internalBuildOptions);
    bool moveOptLevelOption(std::string &dstOptionsSet, std::string &srcOptionSet);
    bool moveProfileFlagsOption(std::string &dstOptionsSet, std::string &srcOptionSet);
    void updateBuildLog(NEO::Device *neoDevice);

    Device *getDevice() const override { return device; }

    MOCKABLE_VIRTUAL bool linkBinary();

    ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice);

    bool isDebugEnabled() const override;

    bool isSPIRv() { return builtFromSPIRv; }

    bool shouldAllocatePrivateMemoryPerDispatch() const override {
        return allocatePrivateMemoryPerDispatch;
    }

    uint32_t getProfileFlags() const override { return profileFlags; }

    ModuleTranslationUnit *getTranslationUnit() {
        return this->translationUnit.get();
    }

  protected:
    void copyPatchedSegments(const NEO::Linker::PatchableSegments &isaSegmentsForPatching);
    void verifyDebugCapabilities();
    void checkIfPrivateMemoryPerDispatchIsNeeded() override;
    NEO::Debug::Segments getZebinSegments();
    void passDebugData();
    void createDebugZebin();
    void registerElfInDebuggerL0();
    void notifyModuleCreate();
    void notifyModuleDestroy();
    bool populateHostGlobalSymbolsMap(std::unordered_map<std::string, std::string> &devToHostNameMapping);
    StackVec<NEO::GraphicsAllocation *, 32> getModuleAllocations();

    Device *device = nullptr;
    PRODUCT_FAMILY productFamily{};
    std::unique_ptr<ModuleTranslationUnit> translationUnit;
    ModuleBuildLog *moduleBuildLog = nullptr;
    NEO::GraphicsAllocation *exportedFunctionsSurface = nullptr;
    uint32_t maxGroupSize = 0U;
    std::vector<std::unique_ptr<KernelImmutableData>> kernelImmDatas;
    NEO::Linker::RelocatedSymbolsMap symbols;

    struct HostGlobalSymbol {
        uintptr_t address = std::numeric_limits<uintptr_t>::max();
        size_t size = 0U;
    };

    std::unordered_map<std::string, HostGlobalSymbol> hostGlobalSymbolsMap;

    bool builtFromSPIRv = false;
    bool debugEnabled = false;
    bool isFullyLinked = false;
    bool allocatePrivateMemoryPerDispatch = true;
    bool isZebinBinary = false;
    ModuleType type;
    NEO::Linker::UnresolvedExternals unresolvedExternalsInfo{};
    std::set<NEO::GraphicsAllocation *> importedSymbolAllocations{};
    uint32_t debugModuleHandle = 0;
    uint32_t profileFlags = 0;
    uint64_t moduleLoadAddress = std::numeric_limits<uint64_t>::max();

    NEO::Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;
};

bool moveBuildOption(std::string &dstOptionsSet, std::string &srcOptionSet, NEO::ConstStringRef dstOptionName, NEO::ConstStringRef srcOptionName);

} // namespace L0
