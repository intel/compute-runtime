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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::Module> : public ::L0::ModuleImp {
    using BaseClass = ::L0::ModuleImp;
    using BaseClass::BaseClass;
    using BaseClass::device;
    using BaseClass::translationUnit;
};

using Module = WhiteBox<::L0::Module>;

template <>
struct Mock<Module> : public Module {
    Mock() = delete;
    Mock(::L0::Device *device, NEO::Device *neoDevice, ModuleBuildLog *moduleBuildLog);

    MOCK_METHOD2(createKernel,
                 ze_result_t(const ze_kernel_desc_t *desc, ze_kernel_handle_t *phFunction));
    MOCK_METHOD0(destroy, ze_result_t());
    MOCK_METHOD2(getFunctionPointer, ze_result_t(const char *pKernelName, void **pfnFunction));
    MOCK_METHOD2(getNativeBinary, ze_result_t(size_t *pSize, uint8_t *pModuleNativeBinary));
    MOCK_CONST_METHOD1(getKernelImmutableData, const L0::KernelImmutableData *(const char *functionName));
    MOCK_CONST_METHOD0(getMaxGroupSize, uint32_t());
    MOCK_METHOD2(getKernelNames, ze_result_t(uint32_t *pCount, const char **pNames));

    MOCK_METHOD2(getGlobalPointer, ze_result_t(const char *pGlobalName, void **pPtr));
    MOCK_CONST_METHOD0(isDebugEnabled, bool());
};

struct MockModuleTranslationUnit : public L0::ModuleTranslationUnit {
    MockModuleTranslationUnit(L0::Device *device) : L0::ModuleTranslationUnit(device) {
    }

    bool processUnpackedBinary() override {
        return true;
    }
};

struct MockCompilerInterface : public NEO::CompilerInterface {
    MockCompilerInterface(uint32_t moduleNumSpecConstants) : moduleNumSpecConstants(moduleNumSpecConstants) {
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

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
