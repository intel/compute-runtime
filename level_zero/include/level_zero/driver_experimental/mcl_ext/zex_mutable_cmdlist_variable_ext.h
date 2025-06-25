/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ze_stypes.h"
#include <level_zero/ze_api.h>

///////////////////////////////////////////////////////////////////////////////
/// @brief Variable handle
typedef struct _zex_variable_handle_t *zex_variable_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Variable descriptor
typedef struct _zex_variable_desc_t {
    ze_structure_type_ext_t stype = ZEX_STRUCTURE_TYPE_VARIABLE_DESCRIPTOR; ///< [in] type of this structure
    const void *pNext = nullptr;                                            ///< [in][optional] pointer to extension-specific structure

    const char *name = nullptr; ///< [in][optional] null-terminated name of the variable
} zex_variable_desc_t;

#if defined(__cplusplus)
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates/returns variable based on name provided in
///        variable descriptor.
///
/// @details
///     - When variable with the name provided in variable descriptor does not
///       exist new variable is created.
///     - Variable at creation has no type attached to it (can be set to buffer, image, etc).
///     - If variable with provided name exists it's returned.
///
/// @returns
///     - ZE_RESULT_SUCCESS
///     - ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///         + bad_alloc
///     - ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + nullptr == hCmdList
///         + nullptr == pVariableDescriptor
///         + nullptr == phVariable
ze_result_t ZE_APICALL
zexCommandListGetVariable(
    ze_command_list_handle_t hCmdList,              ///< [in] handle of mutable command list
    const zex_variable_desc_t *pVariableDescriptor, ///< [in] pointer to variable descriptor
    zex_variable_handle_t *phVariable               ///< [in,out] pointer to handle of variable
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Sets kernel argument to be a variable.
///
/// @details
///     - Variable will be used as kernel argument at argIndex.
///     - If variable is used for first time sets it's type accordingly,
///       else checks if type is right.
///     - On zeCommandListAppendLaunchKernel, variable will be notified of it's
///       usage in kernel.
///     - On zexVariableSetValue all usages will be patched - kernel argument will be set.
///     - If kernel has not yet been appended using zeCommandListAppendLaunchKernel
///       it can be overriden with zeKernelSetArgumentValue(). Argument stops being a variable.
///
/// @returns
///     - ZE_RESULT_SUCCESS
///     - ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + nullptr == hKernel
///         + nullptr == hVariable
///         + kernelArgType != variable->type
ze_result_t ZE_APICALL
zexKernelSetArgumentVariable(
    ze_kernel_handle_t hKernel,     ///< [in] handle of kernel
    uint32_t argIndex,              ///< [in] argument index in kernel
    zex_variable_handle_t hVariable ///< [in] handle of variable
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Sets variable's value.
///
/// @details
///     - Sets variable's value based on variable's type.
///     - Patches all variable's usages with it's value.
///     - Can be called multiple times to override previous value.
///     - If variable is a buffer:
///       - Adds buffer to residency container
///       - If variable already set remove buffer form residency container
///
/// @returns
///     - ZE_RESULT_SUCCESS
///     - ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + nullptr == hVariable
///         + nullptr == pValue
///         + variable->size != valueSize
///     - ZE_RESULT_UNSUPPORTED_FEATURE
///         + variable->type != Type::Buffer ;for now only works with buffers
ze_result_t ZE_APICALL
zexVariableSetValue(
    zex_variable_handle_t hVariable, ///< [in] handle of variable
    uint32_t flags,                  ///< [in] flags
    size_t valueSize,                ///< [in] size of value
    const void *pValue               ///< [in] pointer to value
);

typedef enum _zex_mcl_alu_reg_t {
    ZE_MCL_ALU_REG_GPR0 = 0,
    ZE_MCL_ALU_REG_GPR0_1 = 1,
    ZE_MCL_ALU_REG_GPR1 = 2,
    ZE_MCL_ALU_REG_GPR1_1 = 3,
    ZE_MCL_ALU_REG_GPR2 = 4,
    ZE_MCL_ALU_REG_GPR2_1 = 5,
    ZE_MCL_ALU_REG_GPR3 = 6,
    ZE_MCL_ALU_REG_GPR3_1 = 7,
    ZE_MCL_ALU_REG_GPR4 = 8,
    ZE_MCL_ALU_REG_GPR4_1 = 9,
    ZE_MCL_ALU_REG_GPR5 = 10,
    ZE_MCL_ALU_REG_GPR5_1 = 11,
    ZE_MCL_ALU_REG_GPR6 = 12,
    ZE_MCL_ALU_REG_GPR6_1 = 13,
    ZE_MCL_ALU_REG_GPR7 = 14,
    ZE_MCL_ALU_REG_GPR7_1 = 15,
    ZE_MCL_ALU_REG_GPR8 = 16,
    ZE_MCL_ALU_REG_GPR8_1 = 17,
    ZE_MCL_ALU_REG_GPR9 = 18,
    ZE_MCL_ALU_REG_GPR9_1 = 19,
    ZE_MCL_ALU_REG_GPR10 = 20,
    ZE_MCL_ALU_REG_GPR10_1 = 21,
    ZE_MCL_ALU_REG_GPR11 = 22,
    ZE_MCL_ALU_REG_GPR11_1 = 23,
    ZE_MCL_ALU_REG_GPR12 = 24,
    ZE_MCL_ALU_REG_GPR12_1 = 25,
    ZE_MCL_ALU_REG_GPR13 = 26,
    ZE_MCL_ALU_REG_GPR13_1 = 27,
    ZE_MCL_ALU_REG_GPR14 = 28,
    ZE_MCL_ALU_REG_GPR14_1 = 29,
    ZE_MCL_ALU_REG_GPR15 = 30,
    ZE_MCL_ALU_REG_GPR15_1 = 31,
    ZE_MCL_ALU_REG_GPR_MAX = 32,
    ZE_MCL_ALU_REG_PREDICATE1 = 33,
    ZE_MCL_ALU_REG_REG_MAX = 36,
    ZE_MCL_ALU_REG_CONST0 = 37,
    ZE_MCL_ALU_REG_CONST1 = 38,
    ZE_MCL_ALU_REG_NONE = 39,
    ZE_MCL_ALU_REG_PREDICATE2 = 40,
    ZE_MCL_ALU_REG_PREDICATE_RESULT = 41,
    ZE_MCL_ALU_REG_MAX = 42
} zex_mcl_alu_reg_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Appends command loading value (DWORD) to GPR register from
///        variable's buffer.
ze_result_t ZE_APICALL
zexCommandListAppendLoadRegVariable(
    ze_command_list_handle_t hCommandList, ///< [in] handle of mutable command list
    zex_mcl_alu_reg_t reg,                 ///< [in] GPR register destination
    zex_variable_handle_t hVariable);      ///< [in] handle of variable

///////////////////////////////////////////////////////////////////////////////
/// @brief Appends command storing value (DWORD) from GPR register to
///        variable's buffer.
ze_result_t ZE_APICALL
zexCommandListAppendStoreRegVariable(
    ze_command_list_handle_t hCommandList, ///< [in] handle of mutable command list
    zex_mcl_alu_reg_t reg,                 ///< [in] GPR register source
    zex_variable_handle_t hVariable);      ///< [in] handle of variable

typedef ze_result_t(ZE_APICALL *zex_pfnKernelSetArgumentVariableCb_t)(
    ze_kernel_handle_t hKernel,
    uint32_t argIndex,
    zex_variable_handle_t hVariable);

typedef ze_result_t(ZE_APICALL *zex_pfnVariableSetValueCb_t)(
    zex_variable_handle_t hVariable,
    uint32_t flags,
    size_t valueSize,
    const void *pValue);

typedef ze_result_t(ZE_APICALL *zex_pfnCommandListAppendLoadRegVariableCb_t)(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable);

typedef ze_result_t(ZE_APICALL *zex_pfnCommandListAppendStoreRegVariableCb_t)(
    ze_command_list_handle_t hCommandList,
    zex_mcl_alu_reg_t reg,
    zex_variable_handle_t hVariable);

#if defined(__cplusplus)
} // extern "C"
#endif
