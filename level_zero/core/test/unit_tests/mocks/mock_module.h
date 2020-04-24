/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
