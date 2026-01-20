/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/compiler_interface/spec_const_values_map.h"
#include "shared/source/program/program_info.h"

#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"

#include "neo_igfxfmid.h"
#include "ocl_igc_interface/code_type.h"

#include <memory>
#include <ranges>
#include <set>
#include <string>

namespace NEO {
struct KernelDescriptor;
struct MetadataGeneration;
struct TranslationInput;
class SharedPoolAllocation;
class Device;
class CompilerInterface;

namespace Zebin::Debug {
struct Segments;
} // namespace Zebin::Debug
} // namespace NEO
namespace L0 {
struct Device;
struct ModuleBuildLog;

namespace BuildOptions {
extern NEO::ConstStringRef optDisable;
extern NEO::ConstStringRef optLevel;
extern NEO::ConstStringRef greaterThan4GbRequired;
extern NEO::ConstStringRef hasBufferOffsetArg;
extern NEO::ConstStringRef debugKernelEnable;
extern NEO::ConstStringRef profileFlags;
extern NEO::ConstStringRef optLargeRegisterFile;
extern NEO::ConstStringRef optAutoGrf;
extern NEO::ConstStringRef enableLibraryCompile;
extern NEO::ConstStringRef enableGlobalVariableSymbols;
extern NEO::ConstStringRef enableFP64GenEmu;

} // namespace BuildOptions

struct ModuleTranslationUnit {
    ModuleTranslationUnit(L0::Device *device);
    virtual ~ModuleTranslationUnit();
    MOCKABLE_VIRTUAL ze_result_t buildFromSpirV(const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                                const ze_module_constants_t *pConstants) {
        return buildFromIntermediate(IGC::CodeType::spirV, input, inputSize, buildOptions, internalBuildOptions, pConstants);
    }

    MOCKABLE_VIRTUAL ze_result_t buildFromSource(ze_module_format_t inputFormat, const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions);
    MOCKABLE_VIRTUAL ze_result_t buildExt(ze_module_format_t inputFormat, const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions);

    MOCKABLE_VIRTUAL ze_result_t buildFromIntermediate(IGC::CodeType::CodeType_t intermediateType, const char *input, uint32_t inputSize, const char *buildOptions, const char *internalBuildOptions,
                                                       const ze_module_constants_t *pConstants);
    MOCKABLE_VIRTUAL ze_result_t staticLinkSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes, const char *buildOptions, const char *internalBuildOptions,
                                                 std::vector<const ze_module_constants_t *> specConstants);
    MOCKABLE_VIRTUAL ze_result_t createFromNativeBinary(const char *input, size_t inputSize, const char *internalBuildOptions);
    MOCKABLE_VIRTUAL ze_result_t processUnpackedBinary();
    std::vector<uint8_t> generateElfFromSpirV(std::vector<const char *> inputSpirVs, std::vector<uint32_t> inputModuleSizes);
    bool processSpecConstantInfo(NEO::CompilerInterface *compilerInterface, const ze_module_constants_t *pConstants, const char *input, uint32_t inputSize);
    std::string generateCompilerOptions(const char *buildOptions, const char *internalBuildOptions);
    MOCKABLE_VIRTUAL ze_result_t compileGenBinary(NEO::TranslationInput &inputArgs, bool staticLink);
    void updateBuildLog(const std::string &newLogEntry);
    void processDebugData();
    void freeGlobalBufferAllocation(std::unique_ptr<NEO::SharedPoolAllocation> &buffer);
    NEO::GraphicsAllocation *getGlobalConstBufferGA() const;
    NEO::GraphicsAllocation *getGlobalVarBufferGA() const;

    L0::Device *device = nullptr;

    std::unique_ptr<NEO::SharedPoolAllocation> globalConstBuffer;
    std::unique_ptr<NEO::SharedPoolAllocation> globalVarBuffer;
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
    bool isGeneratedByIgc{true};
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

    ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *pInspectDesc,
                               uint32_t numModules,
                               ze_module_handle_t *phModules,
                               ze_module_build_log_handle_t *phLog) override;

    ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) override;

    const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override;

    const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const override { return kernelImmData; }
    NEO::GraphicsAllocation *getKernelsIsaParentAllocation() const;

    uint32_t getMaxGroupSize(const NEO::KernelDescriptor &kernelDescriptor) const override;

    void createBuildOptions(const char *pBuildFlags, std::string &buildOptions, std::string &internalBuildOptions);
    void createBuildExtraOptions(std::string &buildOptions, std::string &internalBuildOptions);
    bool verifyBuildOptions(std::string buildOptions) const;
    bool moveOptLevelOption(std::string &dstOptionsSet, std::string &srcOptionSet);
    bool moveProfileFlagsOption(std::string &dstOptionsSet, std::string &srcOptionSet);
    MOCKABLE_VIRTUAL void updateBuildLog(NEO::Device *neoDevice);

    Device *getDevice() const override { return device; }

    MOCKABLE_VIRTUAL bool linkBinary();

    ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override;

    bool isSPIRv() { return builtFromSpirv; }

    bool isPrecompiled() { return precompiled; }

    bool shouldAllocatePrivateMemoryPerDispatch() const override {
        return allocatePrivateMemoryPerDispatch;
    }

    void populateZebinExtendedArgsMetadata() override;
    void generateDefaultExtendedArgsMetadata() override;

    uint32_t getProfileFlags() const override { return profileFlags; }

    ModuleTranslationUnit *getTranslationUnit() {
        return this->translationUnit.get();
    }

    std::vector<std::shared_ptr<Kernel>> &getPrintfKernelContainer() { return this->printfKernelContainer; }
    std::weak_ptr<Kernel> getPrintfKernelWeakPtr(ze_kernel_handle_t kernelHandle) const;
    ze_result_t destroyPrintfKernel(ze_kernel_handle_t kernelHandle);
    ModuleType getModuleType() const {
        return this->type;
    }

    bool isModulesPackage() const override {
        return false;
    }

  protected:
    MOCKABLE_VIRTUAL ze_result_t initializeTranslationUnit(const ze_module_desc_t *desc, NEO::Device *neoDevice);
    bool shouldBuildBeFailed(NEO::Device *neoDevice);
    ze_result_t allocateKernelImmutableData(size_t kernelsCount);
    ze_result_t initializeKernelImmutableData();
    void copyPatchedSegments(const NEO::Linker::PatchableSegments &isaSegmentsForPatching);
    void checkIfPrivateMemoryPerDispatchIsNeeded() override;
    NEO::Zebin::Debug::Segments getZebinSegments();
    void createDebugZebin();
    void registerElfInDebuggerL0();
    void notifyModuleCreate();
    void notifyModuleDestroy();
    bool populateHostGlobalSymbolsMap(std::unordered_map<std::string, std::string> &devToHostNameMapping);
    ze_result_t setIsaGraphicsAllocations();
    void transferIsaSegmentsToAllocation(NEO::Device *neoDevice, const NEO::Linker::PatchableSegments *isaSegmentsForPatching);
    std::pair<const void *, size_t> getKernelHeapPointerAndSize(const std::unique_ptr<KernelImmutableData> &kernelImmData, const NEO::Linker::PatchableSegments *isaSegmentsForPatching);
    MOCKABLE_VIRTUAL NEO::GraphicsAllocation *allocateKernelsIsaMemory(size_t size);
    StackVec<NEO::GraphicsAllocation *, 32> getModuleAllocations();
    size_t getIsaAllocationPageSize() const;

    Device *device = nullptr;
    PRODUCT_FAMILY productFamily{};
    std::unique_ptr<ModuleTranslationUnit> translationUnit;
    ModuleBuildLog *moduleBuildLog = nullptr;
    NEO::GraphicsAllocation *exportedFunctionsSurface = nullptr;
    NEO::GraphicsAllocation *kernelsIsaParentRegion = nullptr;
    std::unique_ptr<NEO::SharedPoolAllocation> sharedIsaAllocation;
    std::vector<std::shared_ptr<Kernel>> printfKernelContainer;
    std::vector<std::unique_ptr<KernelImmutableData>> kernelImmData;
    NEO::Linker::RelocatedSymbolsMap symbols;

    struct HostGlobalSymbol {
        uintptr_t address = std::numeric_limits<uintptr_t>::max();
        size_t size = 0U;
    };

    std::unordered_map<std::string, HostGlobalSymbol> hostGlobalSymbolsMap;

    bool builtFromSpirv = false;
    bool isFullyLinked = false;
    bool allocatePrivateMemoryPerDispatch = true;
    bool isZebinBinary = false;
    bool isFunctionSymbolExportEnabled = false;
    bool isGlobalSymbolExportEnabled = false;
    bool precompiled = false;
    ModuleType type;
    NEO::Linker::UnresolvedExternals unresolvedExternalsInfo{};
    std::set<NEO::GraphicsAllocation *> importedSymbolAllocations{};
    uint32_t debugModuleHandle = 0;
    uint32_t debugElfHandle = 0;
    uint32_t profileFlags = 0;
    uint64_t moduleLoadAddress = std::numeric_limits<uint64_t>::max();
    size_t isaAllocationPageSize = 0;

    NEO::Linker::PatchableSegments isaSegmentsForPatching;
    std::vector<std::vector<char>> patchedIsaTempStorage;

    std::unique_ptr<NEO::MetadataGeneration> metadataGeneration;
};

struct ModulesPackage : public Module {
    struct ReturnsSuccess {
        static bool isTrue(ze_result_t returnValue) {
            return ZE_RESULT_SUCCESS == returnValue;
        }

        template <typename T>
        constexpr static T defaultResult() {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    };

    struct ReturnsFailure {
        static bool isTrue(ze_result_t returnValue) {
            return ZE_RESULT_SUCCESS != returnValue;
        }

        template <typename T>
        constexpr static T defaultResult() {
            return ZE_RESULT_SUCCESS;
        }
    };

    struct ReturnsNotNull {
        static bool isTrue(const void *returnValue) {
            return nullptr != returnValue;
        }

        template <typename T>
        constexpr static T defaultResult() {
            return static_cast<T>(nullptr);
        }
    };

    ModulesPackage(Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type) : device(device), packageBuildLog(moduleBuildLog), type(type) {
    }

    ze_result_t initialize(const ze_module_desc_t *desc, NEO::Device *neoDevice) override;

    Device *getDevice() const override { return device; }
    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t createKernel(const ze_kernel_desc_t *desc,
                             ze_kernel_handle_t *kernelHandle) override {
        return anyModuleThat<ReturnsSuccess>([&](Module &mod) { return mod.createKernel(desc, kernelHandle); });
    }

    ze_result_t getFunctionPointer(const char *pKernelName, void **pfnFunction) override {
        return anyModuleThat<ReturnsSuccess>([&](Module &mod) { return mod.getFunctionPointer(pKernelName, pfnFunction); });
    }

    ze_result_t getGlobalPointer(const char *pGlobalName, size_t *pSize, void **pPtr) override {
        return anyModuleThat<ReturnsSuccess>([&](Module &mod) { return mod.getGlobalPointer(pGlobalName, pSize, pPtr); });
    }

    const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override {
        return anyModuleThat<ReturnsNotNull>([&](Module &mod) { return mod.getKernelImmutableData(kernelName); });
    }

    ze_result_t getNativeBinary(size_t *pSize, uint8_t *pModuleNativeBinary) override;

    ze_result_t getDebugInfo(size_t *pDebugDataSize, uint8_t *pDebugData) override;

    ze_result_t getKernelNames(uint32_t *pCount, const char **pNames) override {
        if (0 == *pCount) { // accumulate sizes
            return allModulesUnless<ReturnsFailure>([&](Module &mod) {
                uint32_t count = 0;
                auto ret = mod.getKernelNames(&count, nullptr);
                *pCount += count;
                return ret;
            });
        } else { // accumulate names
            uint32_t spaceLeft = *pCount;
            *pCount = 0;
            return allModulesUnless<ReturnsFailure>([&](Module &mod) {
                uint32_t count = spaceLeft;
                auto ret = mod.getKernelNames(&count, pNames + *pCount);
                count = std::min(spaceLeft, count);
                spaceLeft = spaceLeft - count;
                *pCount += count;
                return ret;
            });
        }
    }

    ze_result_t getProperties(ze_module_properties_t *pModuleProperties) override {
        pModuleProperties->flags = 0;
        return allModulesUnless<ReturnsFailure>([&](Module &mod) {
            ze_module_properties_t unitProperties = {ZE_STRUCTURE_TYPE_MODULE_PROPERTIES};
            auto ret = mod.getProperties(&unitProperties);
            pModuleProperties->flags |= unitProperties.flags;
            DEBUG_BREAK_IF((pModuleProperties->flags != 0) && (pModuleProperties->flags != ZE_MODULE_PROPERTY_FLAG_IMPORTS));
            DEBUG_BREAK_IF(pModuleProperties->pNext);
            return ret;
        });
    }

    ze_result_t performDynamicLink(uint32_t numModules,
                                   ze_module_handle_t *phModules,
                                   ze_module_build_log_handle_t *phLinkLog) override;

    ze_result_t inspectLinkage(ze_linkage_inspection_ext_desc_t *pInspectDesc,
                               uint32_t numModules,
                               ze_module_handle_t *phModules,
                               ze_module_build_log_handle_t *phLog) override;

    const std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVector() const override {
        UNRECOVERABLE_IF(true);
    }

    uint32_t getMaxGroupSize(const NEO::KernelDescriptor &kernelDescriptor) const override {
        UNRECOVERABLE_IF(true);
    }

    bool shouldAllocatePrivateMemoryPerDispatch() const override {
        UNRECOVERABLE_IF(true);
    }

    uint32_t getProfileFlags() const override {
        UNRECOVERABLE_IF(true);
    }

    void checkIfPrivateMemoryPerDispatchIsNeeded() override {
        UNRECOVERABLE_IF(true);
    }

    void populateZebinExtendedArgsMetadata() override {
        UNRECOVERABLE_IF(true);
    }

    void generateDefaultExtendedArgsMetadata() override {
        UNRECOVERABLE_IF(true);
    }

    bool isModulesPackage() const override {
        return true;
    }

    static bool isModulesPackageInput(const ze_module_desc_t *desc);

    template <typename T>
    static void gatherAllUnderlyingModuleHandles(std::span<T> in, std::vector<ze_module_handle_t> &out) {
        out.reserve(out.size() + in.size());
        for (auto &modHandle : in) {
            auto *mod = Module::fromHandle(&*modHandle);
            if (mod->isModulesPackage()) {
                auto childPackage = static_cast<ModulesPackage *>(mod);
                gatherAllUnderlyingModuleHandles(std::span(childPackage->modules), out);
            } else {
                out.push_back(mod->toHandle());
            }
        }
    }

  protected:
    // Sequentially invokes callabale on modules, stops at first module which return value passes ValidatorT::isTrue
    template <typename ValidatorT, typename CallableT, typename ReturnT = std::invoke_result_t<CallableT, ModuleImp &>>
    auto anyModuleThat(const CallableT &callable) const -> ReturnT {
        ReturnT res = ValidatorT::template defaultResult<ReturnT>();
        for (auto &mod : modules) {
            res = callable(*mod);
            if (ValidatorT::isTrue(res)) {
                return res;
            }
        }
        return res;
    }

    template <typename ValidatorT, typename CallableT, typename ReturnT = std::invoke_result_t<CallableT, ModuleImp &>>
    auto allModulesUnless(const CallableT &callable) const -> ReturnT {
        return anyModuleThat<ValidatorT, CallableT, ReturnT>(callable);
    }

    MOCKABLE_VIRTUAL std::unique_ptr<Module> createModuleUnit(Device *device, ModuleBuildLog *buildLog, ModuleType type) {
        return std::make_unique<ModuleImp>(this->device, buildLog, this->type);
    }

    void setNativeBinary(std::span<const uint8_t> binary);
    MOCKABLE_VIRTUAL ze_result_t prepareNativeBinary();
    MOCKABLE_VIRTUAL ze_result_t prepareDebugInfo();

    Device *device = nullptr;
    ModuleBuildLog *packageBuildLog = nullptr;
    ModuleType type = ModuleType::builtin;
    std::vector<std::unique_ptr<Module>> modules;
    std::mutex nativeBinaryPrepareLock;
    std::vector<uint8_t> nativeBinary;
    std::vector<uint8_t> debugInfo;
};

bool moveBuildOption(std::string &dstOptionsSet, std::string &srcOptionSet, NEO::ConstStringRef dstOptionName, NEO::ConstStringRef srcOptionName);

} // namespace L0
