/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/mocks/mock_compiler_interface.h"

#include "opencl/test/unit_test/mocks/mock_cif.h"

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
    using BaseClass::isFullyLinked;
    using BaseClass::kernelImmDatas;
    using BaseClass::symbols;
    using BaseClass::translationUnit;
    using BaseClass::unresolvedExternalsInfo;
};

using Module = WhiteBox<::L0::Module>;

template <>
struct Mock<Module> : public Module {
    Mock() = delete;
    Mock(::L0::Device *device, ModuleBuildLog *moduleBuildLog);

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
    MOCK_METHOD(ze_result_t, getGlobalPointer, (const char *pGlobalName, void **pPtr), (override));
    MOCK_METHOD(bool, isDebugEnabled, (), (const, override));
};

struct MockModuleTranslationUnit : public L0::ModuleTranslationUnit {
    MockModuleTranslationUnit(L0::Device *device) : L0::ModuleTranslationUnit(device) {
    }

    bool processUnpackedBinary() override {
        return true;
    }
};

struct MockCompilerInterface : public NEO::CompilerInterface {
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        return NEO::TranslationOutput::ErrorCode::Success;
    }
};

struct MockCompilerInterfaceWithSpecConstants : public NEO::CompilerInterface {
    MockCompilerInterfaceWithSpecConstants(uint32_t moduleNumSpecConstants) : moduleNumSpecConstants(moduleNumSpecConstants) {
    }
    NEO::TranslationOutput::ErrorCode build(const NEO::Device &device,
                                            const NEO::TranslationInput &input,
                                            NEO::TranslationOutput &output) override {

        EXPECT_EQ(moduleNumSpecConstants, input.specializedValues.size());

        for (uint32_t i = 0; i < moduleSpecConstantsIds.size(); i++) {
            uint32_t specConstantId = moduleSpecConstantsIds[i];
            auto it = input.specializedValues.find(specConstantId);
            EXPECT_NE(it, input.specializedValues.end());

            uint64_t specConstantValue = moduleSpecConstantsValues[i];
            EXPECT_EQ(specConstantValue, it->second);
        }

        return NEO::TranslationOutput::ErrorCode::Success;
    }

    NEO::TranslationOutput::ErrorCode getSpecConstantsInfo(const NEO::Device &device,
                                                           ArrayRef<const char> srcSpirV, NEO::SpecConstantInfo &output) override {
        output.idsBuffer.reset(new NEO::MockCIFBuffer());
        for (uint32_t i = 0; i < moduleNumSpecConstants; i++) {
            output.idsBuffer->PushBackRawCopy(moduleSpecConstantsIds[i]);
        }
        return NEO::TranslationOutput::ErrorCode::Success;
    }
    uint32_t moduleNumSpecConstants = 0u;
    const std::vector<uint32_t> moduleSpecConstantsIds{0, 1, 2, 3};
    const std::vector<uint64_t> moduleSpecConstantsValues{10, 20, 30, 40};
};

} // namespace ult
} // namespace L0
