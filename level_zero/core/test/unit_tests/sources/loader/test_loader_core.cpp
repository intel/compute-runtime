/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ddi_table_expectations.h"
#include <level_zero/ze_api.h>

namespace L0 {
namespace ult {

// Single manifest of all L0 core DDI tables. Every entry declared by the L0 headers is listed
// here, in header declaration order, together with the API version that first exposes it.
//
// This is the only place that has to be updated when a ze*GetProcAddrTable() implementation
// gains, drops or re-versions an entry, or when the L0 headers add a new DDI slot. Any such change
// that is not reflected here makes the tests below fail.

namespace ZeDdiTableManifest {

namespace zeDriver {
using Table = ze_driver_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetApiVersion, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetIpcProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetExtensionProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetExtensionFunctionAddress, ZE_API_VERSION_1_1),
    DDI_ENTRY(pfnGetLastErrorDescription, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnRTASFormatCompatibilityCheckExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnGetDefaultContext, ZE_API_VERSION_1_14),
};
} // namespace zeDriver

namespace zeMem {
using Table = ze_mem_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnAllocShared, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAllocDevice, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAllocHost, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnFree, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetAllocProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetAddressRange, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOpenIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCloseIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnFreeExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnPutIpcHandle, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnGetPitchFor2dImage, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetIpcHandleWithProperties, ZE_API_VERSION_1_15),
};
} // namespace zeMem

namespace zeContext {
using Table = ze_context_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetStatus, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSystemBarrier, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnMakeMemoryResident, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEvictMemory, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnMakeImageResident, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnEvictImage, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCreateEx, ZE_API_VERSION_1_1),
};
} // namespace zeContext

namespace zePhysicalMem {
using Table = ze_physical_mem_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_15),
};
} // namespace zePhysicalMem

namespace zeVirtualMem {
using Table = ze_virtual_mem_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnReserve, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnFree, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnQueryPageSize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnMap, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnUnmap, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetAccessAttribute, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetAccessAttribute, ZE_API_VERSION_1_0),
};
} // namespace zeVirtualMem

namespace zeGlobal {
using Table = ze_global_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnInit, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnInitDrivers, ZE_API_VERSION_1_10),
};
} // namespace zeGlobal

namespace zeDevice {
using Table = ze_device_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetSubDevices, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetComputeProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetModuleProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetCommandQueueGroupProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetMemoryProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetMemoryAccessProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetCacheProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetImageProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetExternalMemoryProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetP2PProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCanAccessPeer, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetStatus, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetGlobalTimestamps, ZE_API_VERSION_1_1),
    DDI_ENTRY(pfnReserveCacheExt, ZE_API_VERSION_1_2),
    DDI_ENTRY(pfnSetCacheAdviceExt, ZE_API_VERSION_1_2),
    DDI_ENTRY(pfnPciGetPropertiesExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnGetRootDevice, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnImportExternalSemaphoreExt, ZE_API_VERSION_1_12),
    DDI_ENTRY(pfnReleaseExternalSemaphoreExt, ZE_API_VERSION_1_12),
    DDI_ENTRY(pfnGetVectorWidthPropertiesExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnSynchronize, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnGetAggregatedCopyOffloadIncrementValue, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnGetRuntimeRequirements, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetRuntimeRequirementsKey, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnValidateRuntimeRequirements, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetCounterBasedEventMaxValue, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnGetCompilerInfo, ZE_API_VERSION_1_18),
};
} // namespace zeDevice

namespace zeDeviceExp {
using Table = ze_device_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetFabricVertexExp, ZE_API_VERSION_1_4),
};
} // namespace zeDeviceExp

namespace zeCommandQueue {
using Table = ze_command_queue_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnExecuteCommandLists, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSynchronize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetOrdinal, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetIndex, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetFlags, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnGetMode, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnGetPriority, ZE_API_VERSION_1_17),
};
} // namespace zeCommandQueue

namespace zeCommandList {
using Table = ze_command_list_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCreateImmediate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnClose, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReset, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendWriteGlobalTimestamp, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendBarrier, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryRangesBarrier, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryCopy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryFill, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryCopyRegion, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryCopyFromContext, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendImageCopy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendImageCopyRegion, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendImageCopyToMemory, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendImageCopyFromMemory, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemoryPrefetch, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMemAdvise, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendSignalEvent, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendWaitOnEvents, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendEventReset, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendQueryKernelTimestamps, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendLaunchKernel, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendLaunchCooperativeKernel, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendLaunchKernelIndirect, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendLaunchMultipleKernelsIndirect, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendImageCopyToMemoryExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnAppendImageCopyFromMemoryExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnHostSynchronize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetDeviceHandle, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetContextHandle, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetOrdinal, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnImmediateGetIndex, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnIsImmediate, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnAppendSignalExternalSemaphoreExt, ZE_API_VERSION_1_12),
    DDI_ENTRY(pfnAppendWaitExternalSemaphoreExt, ZE_API_VERSION_1_12),
    DDI_ENTRY(pfnAppendLaunchKernelWithParameters, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnAppendLaunchKernelWithArguments, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnAppendMemoryCopyWithParameters, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnAppendMemoryFillWithParameters, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnImmediateAppendCommandListsWithParameters, ZE_API_VERSION_1_16),
    DDI_ENTRY(pfnGetFlags, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnImmediateGetFlags, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnImmediateGetMode, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnImmediateGetPriority, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnBeginGraphCaptureExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnBeginCaptureIntoGraphExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnIsGraphCaptureEnabledExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnEndGraphCaptureExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnGetGraphExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnAppendGraphExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnAppendHostFunction, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnAppendSignalEventWithParameters, ZE_API_VERSION_1_18),
    DDI_ENTRY(pfnAppendWaitOnEventsWithParameters, ZE_API_VERSION_1_18),
};
} // namespace zeCommandList

namespace zeCommandListExp {
using Table = ze_command_list_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateCloneExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnImmediateAppendCommandListsExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetNextCommandIdExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnUpdateMutableCommandsExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnUpdateMutableCommandSignalEventExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnUpdateMutableCommandWaitEventsExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetNextCommandIdWithKernelsExp, ZE_API_VERSION_1_10),
    DDI_ENTRY(pfnUpdateMutableCommandKernelsExp, ZE_API_VERSION_1_10),
    DDI_ENTRY(pfnIsMutableExp, ZE_API_VERSION_1_17),
};
} // namespace zeCommandListExp

namespace zeGraph {
using Table = ze_graph_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnGetPrimaryCommandListExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnSetDestructionCallbackExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnInstantiateExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnIsEmptyExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnDumpContentsExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnDestroyExt, ZE_API_VERSION_1_17),
};
} // namespace zeGraph

namespace zeExecutableGraph {
using Table = ze_executable_graph_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetSourceGraphExt, ZE_API_VERSION_1_17),
    DDI_ENTRY(pfnDestroyExt, ZE_API_VERSION_1_17),
};
} // namespace zeExecutableGraph

namespace zeFence {
using Table = ze_fence_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnHostSynchronize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnQueryStatus, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReset, ZE_API_VERSION_1_0),
};
} // namespace zeFence

namespace zeEventPool {
using Table = ze_event_pool_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnOpenIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCloseIpcHandle, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnPutIpcHandle, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnGetContextHandle, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetFlags, ZE_API_VERSION_1_9),
};
} // namespace zeEventPool

namespace zeEvent {
using Table = ze_event_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnHostSignal, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnHostSynchronize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnQueryStatus, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnHostReset, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnQueryKernelTimestamp, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnQueryKernelTimestampsExt, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnGetEventPool, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetSignalScope, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetWaitScope, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnCounterBasedCreate, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnCounterBasedGetIpcHandle, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnCounterBasedOpenIpcHandle, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnCounterBasedCloseIpcHandle, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnCounterBasedGetDeviceAddress, ZE_API_VERSION_1_15),
    DDI_ENTRY(pfnGetCounterBasedFlags, ZE_API_VERSION_1_17),
};
} // namespace zeEvent

namespace zeEventExp {
using Table = ze_event_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnQueryTimestampsExp, ZE_API_VERSION_1_2),
};
} // namespace zeEventExp

namespace zeImage {
using Table = ze_image_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetAllocPropertiesExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnViewCreateExt, ZE_API_VERSION_1_5),
};
} // namespace zeImage

namespace zeModule {
using Table = ze_module_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDynamicLink, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetNativeBinary, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetGlobalPointer, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetKernelNames, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetFunctionPointer, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnInspectLinkageExt, ZE_API_VERSION_1_3),
    DDI_ENTRY(pfnGetDeviceHandle, ZE_API_VERSION_1_18),
};
} // namespace zeModule

namespace zeModuleBuildLog {
using Table = ze_module_build_log_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetString, ZE_API_VERSION_1_0),
};
} // namespace zeModuleBuildLog

namespace zeKernel {
using Table = ze_kernel_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetCacheConfig, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetGroupSize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSuggestGroupSize, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSuggestMaxCooperativeGroupCount, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetArgumentValue, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetIndirectAccess, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetIndirectAccess, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetSourceAttributes, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetName, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetModuleHandle, ZE_API_VERSION_1_18),
};
} // namespace zeKernel

namespace zeSampler {
using Table = ze_sampler_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
};
} // namespace zeSampler

namespace zeKernelExp {
using Table = ze_kernel_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnSetGlobalOffsetExp, ZE_API_VERSION_1_1),
    DDI_ENTRY(pfnSchedulingHintExp, ZE_API_VERSION_1_2),
    DDI_ENTRY(pfnGetBinaryExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnGetAllocationPropertiesExp, ZE_API_VERSION_1_14),
};
} // namespace zeKernelExp

namespace zeMemExp {
using Table = ze_mem_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetIpcHandleFromFileDescriptorExp, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnGetFileDescriptorFromIpcHandleExp, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnSetAtomicAccessAttributeExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetAtomicAccessAttributeExp, ZE_API_VERSION_1_7),
};
} // namespace zeMemExp

namespace zeImageExp {
using Table = ze_image_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetMemoryPropertiesExp, ZE_API_VERSION_1_2),
    DDI_ENTRY(pfnViewCreateExp, ZE_API_VERSION_1_2),
    DDI_ENTRY(pfnGetDeviceOffsetExp, ZE_API_VERSION_1_9),
};
} // namespace zeImageExp

namespace zeFabricVertexExp {
using Table = ze_fabric_vertex_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetExp, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetSubVerticesExp, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetPropertiesExp, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetDeviceExp, ZE_API_VERSION_1_4),
};
} // namespace zeFabricVertexExp

namespace zeFabricEdgeExp {
using Table = ze_fabric_edge_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetExp, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetVerticesExp, ZE_API_VERSION_1_4),
    DDI_ENTRY(pfnGetPropertiesExp, ZE_API_VERSION_1_4),
};
} // namespace zeFabricEdgeExp

namespace zeDriverExp {
using Table = ze_driver_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnRTASFormatCompatibilityCheckExp, ZE_API_VERSION_1_7),
};
} // namespace zeDriverExp

namespace zeRTASParallelOperationExp {
using Table = ze_rtas_parallel_operation_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetPropertiesExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnJoinExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_7),
};
} // namespace zeRTASParallelOperationExp

namespace zeRTASBuilderExp {
using Table = ze_rtas_builder_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnGetBuildPropertiesExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnBuildExp, ZE_API_VERSION_1_7),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_7),
};
} // namespace zeRTASBuilderExp

namespace zeRTASBuilder {
using Table = ze_rtas_builder_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnGetBuildPropertiesExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnBuildExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnCommandListAppendCopyExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnDestroyExt, ZE_API_VERSION_1_13),
};
} // namespace zeRTASBuilder

namespace zeRTASParallelOperation {
using Table = ze_rtas_parallel_operation_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnGetPropertiesExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnJoinExt, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnDestroyExt, ZE_API_VERSION_1_13),
};
} // namespace zeRTASParallelOperation

constexpr DdiTableExpectation ddiTables[] = {
    DDI_TABLE(zeDriver, zeGetDriverProcAddrTable),
    DDI_TABLE(zeMem, zeGetMemProcAddrTable),
    DDI_TABLE(zeContext, zeGetContextProcAddrTable),
    DDI_TABLE(zePhysicalMem, zeGetPhysicalMemProcAddrTable),
    DDI_TABLE(zeVirtualMem, zeGetVirtualMemProcAddrTable),
    DDI_TABLE(zeGlobal, zeGetGlobalProcAddrTable),
    DDI_TABLE(zeDevice, zeGetDeviceProcAddrTable),
    DDI_TABLE(zeDeviceExp, zeGetDeviceExpProcAddrTable),
    DDI_TABLE(zeCommandQueue, zeGetCommandQueueProcAddrTable),
    DDI_TABLE(zeCommandList, zeGetCommandListProcAddrTable),
    DDI_TABLE(zeCommandListExp, zeGetCommandListExpProcAddrTable),
    DDI_TABLE(zeGraph, zeGetGraphProcAddrTable),
    DDI_TABLE(zeExecutableGraph, zeGetExecutableGraphProcAddrTable),
    DDI_TABLE(zeFence, zeGetFenceProcAddrTable),
    DDI_TABLE(zeEventPool, zeGetEventPoolProcAddrTable),
    DDI_TABLE(zeEvent, zeGetEventProcAddrTable),
    DDI_TABLE(zeEventExp, zeGetEventExpProcAddrTable),
    DDI_TABLE(zeImage, zeGetImageProcAddrTable),
    DDI_TABLE(zeModule, zeGetModuleProcAddrTable),
    DDI_TABLE(zeModuleBuildLog, zeGetModuleBuildLogProcAddrTable),
    DDI_TABLE(zeKernel, zeGetKernelProcAddrTable),
    DDI_TABLE(zeSampler, zeGetSamplerProcAddrTable),
    DDI_TABLE(zeKernelExp, zeGetKernelExpProcAddrTable),
    DDI_TABLE(zeMemExp, zeGetMemExpProcAddrTable),
    DDI_TABLE(zeImageExp, zeGetImageExpProcAddrTable),
    DDI_TABLE(zeFabricVertexExp, zeGetFabricVertexExpProcAddrTable),
    DDI_TABLE(zeFabricEdgeExp, zeGetFabricEdgeExpProcAddrTable),
    DDI_TABLE(zeDriverExp, zeGetDriverExpProcAddrTable),
    DDI_TABLE(zeRTASParallelOperationExp, zeGetRTASParallelOperationExpProcAddrTable),
    DDI_TABLE(zeRTASBuilderExp, zeGetRTASBuilderExpProcAddrTable),
    DDI_TABLE(zeRTASBuilder, zeGetRTASBuilderProcAddrTable),
    DDI_TABLE(zeRTASParallelOperation, zeGetRTASParallelOperationProcAddrTable),
};

} // namespace ZeDdiTableManifest

constexpr size_t ddiTablesCount = std::size(ZeDdiTableManifest::ddiTables);
constexpr const DdiTableExpectation *ddiTables = ZeDdiTableManifest::ddiTables;

TEST(ZeDdiTableManifestTest, givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnce) {
    givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnceFunction(ddiTables, ddiTablesCount);
}

TEST(ZeGetProcAddrTableTest, givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulated) {
    givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZeGetProcAddrTableTest, givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulated) {
    givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZeGetProcAddrTableTest, givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturned) {
    givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturnedFunction(ddiTables, ddiTablesCount);
}

} // namespace ult
} // namespace L0
