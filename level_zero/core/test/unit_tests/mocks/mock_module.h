/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_cif.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/core/test/unit_tests/white_box.h"

#include "gmock/gmock.h"
namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Module> : public ::L0::ModuleImp {
    using BaseClass = ::L0::ModuleImp;
    using BaseClass::BaseClass;
    using BaseClass::device;
    using BaseClass::exportedFunctionsSurface;
    using BaseClass::isFullyLinked;
    using BaseClass::kernelImmDatas;
    using BaseClass::symbols;
    using BaseClass::translationUnit;
    using BaseClass::type;
    using BaseClass::unresolvedExternalsInfo;
};

using Module = WhiteBox<::L0::Module>;

template <>
struct Mock<Module> : public Module {
    Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog, ModuleType type);
    Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog) : Mock(device, moduleBuildLog, ModuleType::User){};

    MOCK_METHOD(ze_result_t, createKernel, (const ze_kernel_desc_t *desc, ze_kernel_handle_t *phFunction), (override));
    MOCK_METHOD(ze_result_t, destroy, (), (override));
    MOCK_METHOD(ze_result_t, getFunctionPointer, (const char *pKernelName, void **pfnFunction), (override));
    MOCK_METHOD(ze_result_t, getNativeBinary, (size_t * pSize, uint8_t *pModuleNativeBinary), (override));
    MOCK_METHOD(const L0::KernelImmutableData *, getKernelImmutableData, (const char *functionName), (const, override));
    MOCK_METHOD(uint32_t, getMaxGroupSize, (), (const, override));
    MOCK_METHOD(ze_result_t, getKernelNames, (uint32_t * pCount, const char **pNames), (override));
    MOCK_METHOD(ze_result_t, performDynamicLink,
                (uint32_t numModules, ze_module_handle_t *phModules, ze_module_build_log_handle_t *phLinkLog),
                (override));
    MOCK_METHOD(ze_result_t, getProperties, (ze_module_properties_t * pModuleProperties), (override));
    MOCK_METHOD(ze_result_t, getGlobalPointer, (const char *pGlobalName, size_t *pSize, void **pPtr), (override));
    MOCK_METHOD(bool, isDebugEnabled, (), (const, override));
};

struct MockModuleTranslationUnit : public L0::ModuleTranslationUnit {
    MockModuleTranslationUnit(L0::Device *device) : L0::ModuleTranslationUnit(device) {
    }

    bool processUnpackedBinary() override {
        return true;
    }
};

struct MockModule : public L0::ModuleImp {
    using ModuleImp::debugEnabled;
    using ModuleImp::kernelImmDatas;
    using ModuleImp::translationUnit;

    MockModule(L0::Device *device,
               L0::ModuleBuildLog *moduleBuildLog,
               L0::ModuleType type) : ModuleImp(device, moduleBuildLog, type) {
        maxGroupSize = 32;
    };

    ~MockModule() = default;

    const KernelImmutableData *getKernelImmutableData(const char *functionName) const override {
        return kernelImmData;
    }

    KernelImmutableData *kernelImmData = nullptr;
};

struct MockCompilerInterface : public NEO::CompilerInterface {
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        return NEO::TranslationOutput::ErrorCode::Success;
    }
};
template <typename T1, typename T2>
struct MockCompilerInterfaceWithSpecConstants : public NEO::CompilerInterface {
    MockCompilerInterfaceWithSpecConstants(uint32_t moduleNumSpecConstants) : moduleNumSpecConstants(moduleNumSpecConstants) {
    }
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        EXPECT_EQ(moduleNumSpecConstants, input.specializedValues.size());

        return NEO::TranslationOutput::ErrorCode::Success;
    }

    NEO::TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                           ArrayRef<const char> srcSpirV, NEO::SpecConstantInfo &output) override {
        output.idsBuffer.reset(new NEO::MockCIFBuffer());
        output.sizesBuffer.reset(new NEO::MockCIFBuffer());
        for (uint32_t i = 0; i < moduleNumSpecConstants; i++) {
            output.idsBuffer->PushBackRawCopy(moduleSpecConstantsIds[i]);
            output.sizesBuffer->PushBackRawCopy(moduleSpecConstantsSizes[i]);
        }
        return NEO::TranslationOutput::ErrorCode::Success;
    }
    uint32_t moduleNumSpecConstants = 0u;
    const std::vector<uint32_t> moduleSpecConstantsIds{2, 0, 1, 3, 5, 4};
    const std::vector<T1> moduleSpecConstantsValuesT1{10, 20, 30};
    const std::vector<T2> moduleSpecConstantsValuesT2{static_cast<T2>(std::numeric_limits<T1>::max()) + 60u, static_cast<T2>(std::numeric_limits<T1>::max()) + 50u, static_cast<T2>(std::numeric_limits<T1>::max()) + 40u};
    const std::vector<uint32_t> moduleSpecConstantsSizes{sizeof(T2), sizeof(T1), sizeof(T2), sizeof(T1), sizeof(T2), sizeof(T1)};
    static_assert(sizeof(T1) < sizeof(T2));
};

} // namespace ult
} // namespace L0
