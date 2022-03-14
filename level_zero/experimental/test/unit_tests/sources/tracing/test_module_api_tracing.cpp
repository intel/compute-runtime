/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnCreate =
        [](ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_module_desc_t *pDesc, ze_module_handle_t *phModule, ze_module_build_log_handle_t *phBuildLog) { return ZE_RESULT_SUCCESS; };
    ze_module_desc_t desc = {};
    ze_module_handle_t phModule = {};
    ze_module_build_log_handle_t phBuildLog = {};

    prologCbs.Module.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleCreate_Tracing(nullptr, nullptr, &desc, &phModule, &phBuildLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnDestroy =
        [](ze_module_handle_t hModule) { return ZE_RESULT_SUCCESS; };

    prologCbs.Module.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleDestroy_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleBuildLogDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnDestroy =
        [](ze_module_build_log_handle_t hModuleBuildLog) { return ZE_RESULT_SUCCESS; };

    prologCbs.ModuleBuildLog.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.ModuleBuildLog.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleBuildLogDestroy_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleBuildLogGetStringTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.ModuleBuildLog.pfnGetString =
        [](ze_module_build_log_handle_t hModuleBuildLog, size_t *pSize, char *pBuildLog) { return ZE_RESULT_SUCCESS; };

    size_t pSize = {};
    char pBuildLog = {};

    prologCbs.ModuleBuildLog.pfnGetStringCb = genericPrologCallbackPtr;
    epilogCbs.ModuleBuildLog.pfnGetStringCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleBuildLogGetString_Tracing(nullptr, &pSize, &pBuildLog);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleGetNativeBinaryTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnGetNativeBinary =
        [](ze_module_handle_t hModule, size_t *pSize, uint8_t *pModuleNativeBinary) { return ZE_RESULT_SUCCESS; };
    size_t pSize = {};
    uint8_t pModuleNativeBinary = {};

    prologCbs.Module.pfnGetNativeBinaryCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnGetNativeBinaryCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetNativeBinary_Tracing(nullptr, &pSize, &pModuleNativeBinary);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleGetGlobalPointerTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnGetGlobalPointer =
        [](ze_module_handle_t hModule, const char *pGlobalName, size_t *pSize, void **pPtr) { return ZE_RESULT_SUCCESS; };
    const char pGlobalName = {};
    size_t size;
    void *pptr = nullptr;

    prologCbs.Module.pfnGetGlobalPointerCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnGetGlobalPointerCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetGlobalPointer_Tracing(nullptr, &pGlobalName, &size, &pptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelCreateTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnCreate =
        [](ze_module_handle_t hModule, const ze_kernel_desc_t *pDesc, ze_kernel_handle_t *phKernel) { return ZE_RESULT_SUCCESS; };
    const ze_kernel_desc_t desc = {};
    ze_kernel_handle_t phKernel = {};

    prologCbs.Kernel.pfnCreateCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnCreateCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelCreate_Tracing(nullptr, &desc, &phKernel);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelDestroyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnDestroy =
        [](ze_kernel_handle_t hKernel) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnDestroyCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnDestroyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelDestroy_Tracing(nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleGetFunctionPointerTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnGetFunctionPointer =
        [](ze_module_handle_t hModule, const char *pKernelName, void **pfnFunction) { return ZE_RESULT_SUCCESS; };
    const char pKernelName = {};
    void *pfnFunction = nullptr;

    prologCbs.Module.pfnGetFunctionPointerCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnGetFunctionPointerCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetFunctionPointer_Tracing(nullptr, &pKernelName, &pfnFunction);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelSetGroupSizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSetGroupSize =
        [](ze_kernel_handle_t hKernel, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) { return ZE_RESULT_SUCCESS; };
    uint32_t groupSizeX = {};
    uint32_t groupSizeY = {};
    uint32_t groupSizeZ = {};

    prologCbs.Kernel.pfnSetGroupSizeCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSetGroupSizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSetGroupSize_Tracing(nullptr, groupSizeX, groupSizeY, groupSizeZ);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelSuggestGroupSizeTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSuggestGroupSize =
        [](ze_kernel_handle_t hKernel, uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ, uint32_t *groupSizeX, uint32_t *groupSizeY, uint32_t *groupSizeZ) { return ZE_RESULT_SUCCESS; };
    uint32_t globalSizeX = {};
    uint32_t globalSizeY = {};
    uint32_t globalSizeZ = {};
    uint32_t groupSizeX = {};
    uint32_t groupSizeY = {};
    uint32_t groupSizeZ = {};

    prologCbs.Kernel.pfnSuggestGroupSizeCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSuggestGroupSizeCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSuggestGroupSize_Tracing(nullptr, globalSizeX, globalSizeY, globalSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelSetArgumentValueTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSetArgumentValue =
        [](ze_kernel_handle_t hKernel, uint32_t argIndex, size_t argSize, const void *pArgValue) { return ZE_RESULT_SUCCESS; };
    uint32_t argIndex = {};
    size_t argSize = {};
    const void *pArgValue = nullptr;

    prologCbs.Kernel.pfnSetArgumentValueCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSetArgumentValueCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSetArgumentValue_Tracing(nullptr, argIndex, argSize, &pArgValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnGetProperties =
        [](ze_kernel_handle_t hKernel, ze_kernel_properties_t *pKernelProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnGetPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnGetPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelGetProperties_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingCommandListAppendLaunchKernelTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernel =
        [](ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pLaunchFuncArgs,
           ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    const ze_group_count_t pLaunchFuncArgs = {};
    ze_event_handle_t hSignalEvent = {};
    uint32_t numWaitEvents = {};
    ze_event_handle_t phWaitEvents = {};

    prologCbs.CommandList.pfnAppendLaunchKernelCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendLaunchKernelCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendLaunchKernel_Tracing(nullptr, nullptr, &pLaunchFuncArgs, hSignalEvent, numWaitEvents, &phWaitEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingCommandListAppendLaunchKernelIndirectTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchKernelIndirect =
        [](ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pLaunchArgumentsBuffer,
           ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    const ze_group_count_t pLaunchArgumentsBuffer = {};
    ze_event_handle_t hSignalEvent = {};
    uint32_t numWaitEvents = {};
    ze_event_handle_t phWaitEvents = {};

    prologCbs.CommandList.pfnAppendLaunchKernelIndirectCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendLaunchKernelIndirectCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendLaunchKernelIndirect_Tracing(nullptr, nullptr, &pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, &phWaitEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingCommandListAppendLaunchMultipleKernelsIndirectTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchMultipleKernelsIndirect =
        [](ze_command_list_handle_t hCommandList, uint32_t numKernels, ze_kernel_handle_t *phKernels,
           const uint32_t *pNumLaunchArguments, const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    uint32_t numKernels = {};
    ze_kernel_handle_t phKernels = {};
    const uint32_t pNumLaunchArguments = {};
    const ze_group_count_t pLaunchArgumentsBuffer = {};
    ze_event_handle_t hSignalEvent = {};
    uint32_t numWaitEvents = {};
    ze_event_handle_t phWaitEvents = {};

    prologCbs.CommandList.pfnAppendLaunchMultipleKernelsIndirectCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendLaunchMultipleKernelsIndirectCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendLaunchMultipleKernelsIndirect_Tracing(nullptr, numKernels, &phKernels, &pNumLaunchArguments, &pLaunchArgumentsBuffer, hSignalEvent, numWaitEvents, &phWaitEvents);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingCommandListAppendLaunchCooperativeKernelTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendLaunchCooperativeKernel =
        [](ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pLaunchFuncArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandList.pfnAppendLaunchCooperativeKernelCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendLaunchCooperativeKernelCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendLaunchCooperativeKernel_Tracing(nullptr, nullptr, nullptr, nullptr, 1, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleGetKernelNamesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnGetKernelNames =
        [](ze_module_handle_t hDevice, uint32_t *pCount, const char **pNames) { return ZE_RESULT_SUCCESS; };

    prologCbs.Module.pfnGetKernelNamesCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnGetKernelNamesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetKernelNames_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelSuggestMaxCooperativeGroupCountTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSuggestMaxCooperativeGroupCount =
        [](ze_kernel_handle_t hKernel, uint32_t *totalGroupCount) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnSuggestMaxCooperativeGroupCountCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSuggestMaxCooperativeGroupCountCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSuggestMaxCooperativeGroupCount_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelGetIndirectAccessTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnGetIndirectAccess =
        [](ze_kernel_handle_t hKernel, ze_kernel_indirect_access_flags_t *pFlags) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnGetIndirectAccessCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnGetIndirectAccessCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelGetIndirectAccess_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelGetNameTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnGetName =
        [](ze_kernel_handle_t hKernel, size_t *pSize, char *pName) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnGetNameCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnGetNameCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelGetName_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelGetSourceAttributesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnGetSourceAttributes =
        [](ze_kernel_handle_t hKernel, uint32_t *pSize, char **pString) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnGetSourceAttributesCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnGetSourceAttributesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelGetSourceAttributes_Tracing(nullptr, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingKernelSetIndirectAccessTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Kernel.pfnSetIndirectAccess =
        [](ze_kernel_handle_t hKernel, ze_kernel_indirect_access_flags_t flags) { return ZE_RESULT_SUCCESS; };

    prologCbs.Kernel.pfnSetIndirectAccessCb = genericPrologCallbackPtr;
    epilogCbs.Kernel.pfnSetIndirectAccessCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeKernelSetIndirectAccess_Tracing(nullptr, ZE_KERNEL_INDIRECT_ACCESS_FLAG_HOST);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleDynamicLinkTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnDynamicLink =
        [](uint32_t numModules, ze_module_handle_t *phModules, ze_module_build_log_handle_t *phLinkLog) { return ZE_RESULT_SUCCESS; };

    prologCbs.Module.pfnDynamicLinkCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnDynamicLinkCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleDynamicLink_Tracing(1U, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(zeAPITracingRuntimeTests, WhenCallingModuleGetPropertiesTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result;
    driver_ddiTable.core_ddiTable.Module.pfnGetProperties =
        [](ze_module_handle_t hModule, ze_module_properties_t *pModuleProperties) { return ZE_RESULT_SUCCESS; };

    prologCbs.Module.pfnGetPropertiesCb = genericPrologCallbackPtr;
    epilogCbs.Module.pfnGetPropertiesCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeModuleGetProperties_Tracing(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
