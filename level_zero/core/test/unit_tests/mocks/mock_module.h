/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_cif.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gtest/gtest.h"

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;
struct KernelImmutableData;
struct ModuleBuildLog;

namespace ult {
template <typename Type>
struct Mock;
template <typename Type>
struct WhiteBox;

struct MockModuleTranslationUnit : public L0::ModuleTranslationUnit {
    using BaseClass = L0::ModuleTranslationUnit;
    using BaseClass::isGeneratedByIgc;

    MockModuleTranslationUnit(L0::Device *device) : BaseClass{device} {}

    ADDMETHOD(processUnpackedBinary, ze_result_t, true, ZE_RESULT_SUCCESS, (), ());

    ze_result_t compileGenBinary(NEO::TranslationInput &inputArgs, bool staticLink) override {
        if (unpackedDeviceBinarySize && unpackedDeviceBinary) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ModuleTranslationUnit::compileGenBinary(inputArgs, staticLink);
        }
    }

    void setDummyKernelInfo() {
        this->programInfo.kernelInfos.push_back(dummyKernelInfo.get());
    }

    std::unique_ptr<NEO::KernelInfo> dummyKernelInfo = {};
};

constexpr inline MockModuleTranslationUnit *toMockPtr(L0::ModuleTranslationUnit *tu) {
    return static_cast<MockModuleTranslationUnit *>(tu);
}

template <>
struct WhiteBox<::L0::Module> : public ::L0::ModuleImp {
    using BaseClass = ::L0::ModuleImp;
    using BaseClass::allocateKernelsIsaMemory;
    using BaseClass::allocatePrivateMemoryPerDispatch;
    using BaseClass::BaseClass;
    using BaseClass::builtFromSpirv;
    using BaseClass::checkIfPrivateMemoryPerDispatchIsNeeded;
    using BaseClass::copyPatchedSegments;
    using BaseClass::device;
    using BaseClass::exportedFunctionsSurface;
    using BaseClass::importedSymbolAllocations;
    using BaseClass::isaSegmentsForPatching;
    using BaseClass::isFullyLinked;
    using BaseClass::isFunctionSymbolExportEnabled;
    using BaseClass::isGlobalSymbolExportEnabled;
    using BaseClass::kernelImmDatas;
    using BaseClass::setIsaGraphicsAllocations;
    using BaseClass::symbols;
    using BaseClass::translationUnit;
    using BaseClass::type;
    using BaseClass::unresolvedExternalsInfo;

    WhiteBox(Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type)
        : ::L0::ModuleImp{device, moduleBuildLog, type} {
        this->translationUnit.reset(new MockModuleTranslationUnit{device});
    }

    ze_result_t initializeTranslationUnit(const ze_module_desc_t *desc, NEO::Device *neoDevice) override;

    NEO::GraphicsAllocation *mockGlobalVarBuffer = nullptr;
    NEO::GraphicsAllocation *mockGlobalConstBuffer = nullptr;
};

using Module = WhiteBox<::L0::Module>;

template <>
struct Mock<Module> : public Module {

    Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type) : WhiteBox(device, moduleBuildLog, type) {}
    Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog) : Mock(device, moduleBuildLog, ModuleType::user){};

    ADDMETHOD_NOBASE(createKernel, ze_result_t, ZE_RESULT_SUCCESS, (const ze_kernel_desc_t *desc, ze_kernel_handle_t *kernelHandle));
    ADDMETHOD_NOBASE(destroy, ze_result_t, ZE_RESULT_SUCCESS, ());
    ADDMETHOD_NOBASE(getFunctionPointer, ze_result_t, ZE_RESULT_SUCCESS, (const char *pKernelName, void **pfnFunction));
    ADDMETHOD_NOBASE(getNativeBinary, ze_result_t, ZE_RESULT_SUCCESS, (size_t * pSize, uint8_t *pModuleNativeBinary));
    ADDMETHOD_CONST_NOBASE(getKernelImmutableData, const L0::KernelImmutableData *, nullptr, (const char *kernelName));
    ADDMETHOD_CONST_NOBASE(getMaxGroupSize, uint32_t, 256, (const NEO::KernelDescriptor &));
    ADDMETHOD_NOBASE(getKernelNames, ze_result_t, ZE_RESULT_SUCCESS, (uint32_t * pCount, const char **pNames));
    ADDMETHOD_NOBASE(performDynamicLink, ze_result_t, ZE_RESULT_SUCCESS,
                     (uint32_t numModules, ze_module_handle_t *phModules, ze_module_build_log_handle_t *phLinkLog));
    ADDMETHOD_NOBASE(getProperties, ze_result_t, ZE_RESULT_SUCCESS, (ze_module_properties_t * pModuleProperties));
    ADDMETHOD_NOBASE(getGlobalPointer, ze_result_t, ZE_RESULT_SUCCESS, (const char *pGlobalName, size_t *pSize, void **pPtr));
    ADDMETHOD(initializeTranslationUnit, ze_result_t, true, ZE_RESULT_SUCCESS, (const ze_module_desc_t *desc, NEO::Device *neoDevice), (desc, neoDevice));
    ADDMETHOD_NOBASE_VOIDRETURN(updateBuildLog, (NEO::Device * neoDevice));
    ADDMETHOD(allocateKernelsIsaMemory, NEO::GraphicsAllocation *, true, nullptr, (size_t isaSize), (isaSize));
    ADDMETHOD(computeKernelIsaAllocationAlignedSizeWithPadding, size_t, true, 0ul, (size_t isaSize, bool lastKernel), (isaSize, lastKernel));
};

struct MockModule : public L0::ModuleImp {
    using ModuleImp::allocateKernelImmutableDatas;
    using ModuleImp::allocateKernelsIsaMemory;
    using ModuleImp::computeKernelIsaAllocationAlignedSizeWithPadding;
    using ModuleImp::debugModuleHandle;
    using ModuleImp::getModuleAllocations;
    using ModuleImp::initializeKernelImmutableDatas;
    using ModuleImp::isaAllocationPageSize;
    using ModuleImp::isFunctionSymbolExportEnabled;
    using ModuleImp::isGlobalSymbolExportEnabled;
    using ModuleImp::kernelImmDatas;
    using ModuleImp::populateHostGlobalSymbolsMap;
    using ModuleImp::setIsaGraphicsAllocations;
    using ModuleImp::symbols;
    using ModuleImp::translationUnit;

    MockModule(L0::Device *device,
               L0::ModuleBuildLog *moduleBuildLog,
               L0::ModuleType type) : ModuleImp(device, moduleBuildLog, type) {
        this->translationUnit.reset(new MockModuleTranslationUnit{device});
    };

    ~MockModule() override = default;

    const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override {
        return kernelImmData;
    }

    std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmutableDataVectorRef() { return kernelImmDatas; }

    KernelImmutableData *kernelImmData = nullptr;
};

struct MockCompilerInterface : public NEO::CompilerInterface {
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        receivedApiOptions = input.apiOptions.begin();
        inputInternalOptions = input.internalOptions.begin();

        cachingPassed = input.allowCaching;

        if (failBuild) {
            return NEO::TranslationOutput::ErrorCode::buildFailure;
        }
        return NEO::TranslationOutput::ErrorCode::success;
    }
    NEO::TranslationOutput::ErrorCode link(const NEO::Device &device,
                                           const NEO::TranslationInput &input,
                                           NEO::TranslationOutput &output) override {

        receivedApiOptions = input.apiOptions.begin();
        inputInternalOptions = input.internalOptions.begin();

        return NEO::TranslationOutput::ErrorCode::success;
    }

    std::string receivedApiOptions;
    std::string inputInternalOptions;
    bool failBuild = false;
    bool cachingPassed = false;
};
template <typename T1, typename T2>
struct MockCompilerInterfaceWithSpecConstants : public NEO::CompilerInterface {
    MockCompilerInterfaceWithSpecConstants(uint32_t moduleNumSpecConstants) : moduleNumSpecConstants(moduleNumSpecConstants) {
    }
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        EXPECT_EQ(moduleNumSpecConstants, input.specializedValues.size());

        return NEO::TranslationOutput::ErrorCode::success;
    }
    NEO::TranslationOutput::ErrorCode link(const NEO::Device &device,
                                           const NEO::TranslationInput &input,
                                           NEO::TranslationOutput &output) override {

        EXPECT_EQ(moduleNumSpecConstants, input.specializedValues.size());

        return NEO::TranslationOutput::ErrorCode::success;
    }

    NEO::TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                           ArrayRef<const char> srcSpirV, NEO::SpecConstantInfo &output) override {
        output.idsBuffer.reset(new NEO::MockCIFBuffer());
        output.sizesBuffer.reset(new NEO::MockCIFBuffer());
        for (uint32_t i = 0; i < moduleNumSpecConstants; i++) {
            output.idsBuffer->PushBackRawCopy(moduleSpecConstantsIds[i]);
            output.sizesBuffer->PushBackRawCopy(moduleSpecConstantsSizes[i]);
        }
        return NEO::TranslationOutput::ErrorCode::success;
    }
    uint32_t moduleNumSpecConstants = 0u;
    const std::vector<uint32_t> moduleSpecConstantsIds{2, 0, 1, 3, 5, 4};
    const std::vector<T1> moduleSpecConstantsValuesT1{10, 20, 30};
    const std::vector<T2> moduleSpecConstantsValuesT2{static_cast<T2>(std::numeric_limits<T1>::max()) + 60u, static_cast<T2>(std::numeric_limits<T1>::max()) + 50u, static_cast<T2>(std::numeric_limits<T1>::max()) + 40u};
    const std::vector<uint32_t> moduleSpecConstantsSizes{sizeof(T2), sizeof(T1), sizeof(T2), sizeof(T1), sizeof(T2), sizeof(T1)};
    static_assert(sizeof(T1) < sizeof(T2));
};

struct MockCompilerInterfaceLinkFailure : public NEO::CompilerInterface {
    NEO::TranslationOutput::ErrorCode link(const NEO::Device &device,
                                           const NEO::TranslationInput &input,
                                           NEO::TranslationOutput &output) override {

        return NEO::TranslationOutput::ErrorCode::buildFailure;
    }
};

} // namespace ult
} // namespace L0
