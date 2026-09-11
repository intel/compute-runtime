/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ddi_table_expectations.h"
#include <level_zero/zet_api.h>

namespace L0 {
namespace ult {

// Single manifest of all L0 tools DDI tables. Every entry declared by the L0 headers is listed
// here, in header declaration order, together with the API version that first exposes it.
//
// This is the only place that has to be updated when a zet*GetProcAddrTable() implementation
// gains, drops or re-versions an entry, or when the L0 headers add a new DDI slot. Any such change
// that is not reflected here makes the tests below fail.

namespace ZetDdiTableManifest {

namespace zetContext {
using Table = zet_context_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnActivateMetricGroups, ZE_API_VERSION_1_0),
};
} // namespace zetContext

namespace zetMetricStreamer {
using Table = zet_metric_streamer_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnOpen, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnClose, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReadData, ZE_API_VERSION_1_0),
};
} // namespace zetMetricStreamer

namespace zetTracerExp {
using Table = zet_tracer_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetPrologues, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetEpilogues, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnSetEnabled, ZE_API_VERSION_1_0),
};
} // namespace zetTracerExp

namespace zetCommandList {
using Table = zet_command_list_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnAppendMetricStreamerMarker, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMetricQueryBegin, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMetricQueryEnd, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAppendMetricMemoryBarrier, ZE_API_VERSION_1_0),
};
} // namespace zetCommandList

namespace zetModule {
using Table = zet_module_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetDebugInfo, ZE_API_VERSION_1_0),
};
} // namespace zetModule

namespace zetKernel {
using Table = zet_kernel_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetProfileInfo, ZE_API_VERSION_1_0),
};
} // namespace zetKernel

namespace zetMetricGroup {
using Table = zet_metric_group_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnCalculateMetricValues, ZE_API_VERSION_1_0),
};
} // namespace zetMetricGroup

namespace zetMetric {
using Table = zet_metric_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGet, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetProperties, ZE_API_VERSION_1_0),
};
} // namespace zetMetric

namespace zetMetricQueryPool {
using Table = zet_metric_query_pool_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
};
} // namespace zetMetricQueryPool

namespace zetMetricQuery {
using Table = zet_metric_query_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreate, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDestroy, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReset, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetData, ZE_API_VERSION_1_0),
};
} // namespace zetMetricQuery

namespace zetDevice {
using Table = zet_device_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetDebugProperties, ZE_API_VERSION_1_0),
};
} // namespace zetDevice

namespace zetDebug {
using Table = zet_debug_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnAttach, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnDetach, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReadEvent, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnAcknowledgeEvent, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnInterrupt, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnResume, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReadMemory, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnWriteMemory, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetRegisterSetProperties, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnReadRegisters, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnWriteRegisters, ZE_API_VERSION_1_0),
    DDI_ENTRY(pfnGetThreadRegisterSetProperties, ZE_API_VERSION_1_5),
};
} // namespace zetDebug

namespace zetMetricGroupExp {
using Table = zet_metric_group_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCalculateMultipleMetricValuesExp, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetGlobalTimestampsExp, ZE_API_VERSION_1_5),
    DDI_ENTRY(pfnGetExportDataExp, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnCalculateMetricExportDataExp, ZE_API_VERSION_1_6),
    DDI_ENTRY(pfnCreateExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnAddMetricExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnRemoveMetricExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnCloseExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_9),
};
} // namespace zetMetricGroupExp

namespace zetMetricProgrammableExp {
using Table = zet_metric_programmable_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetPropertiesExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetParamInfoExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnGetParamValueInfoExp, ZE_API_VERSION_1_9),
};
} // namespace zetMetricProgrammableExp

namespace zetMetricExp {
using Table = zet_metric_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateFromProgrammableExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_9),
    DDI_ENTRY(pfnCreateFromProgrammableExp2, ZE_API_VERSION_1_12),
};
} // namespace zetMetricExp

namespace zetMetricTracerExp {
using Table = zet_metric_tracer_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnEnableExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnDisableExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnReadDataExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnDecodeExp, ZE_API_VERSION_1_11),
};
} // namespace zetMetricTracerExp

namespace zetMetricDecoderExp {
using Table = zet_metric_decoder_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnCreateExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnDestroyExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnGetDecodableMetricsExp, ZE_API_VERSION_1_11),
};
} // namespace zetMetricDecoderExp

namespace zetDeviceExp {
using Table = zet_device_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetConcurrentMetricGroupsExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnCreateMetricGroupsFromMetricsExp, ZE_API_VERSION_1_11),
    DDI_ENTRY(pfnEnableMetricsExp, ZE_API_VERSION_1_13),
    DDI_ENTRY(pfnDisableMetricsExp, ZE_API_VERSION_1_13),
};
} // namespace zetDeviceExp

namespace zetCommandListExp {
using Table = zet_command_list_exp_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnAppendMarkerExp, ZE_API_VERSION_1_13),
};
} // namespace zetCommandListExp

constexpr DdiTableExpectation ddiTables[] = {
    DDI_TABLE(zetContext, zetGetContextProcAddrTable),
    DDI_TABLE(zetMetricStreamer, zetGetMetricStreamerProcAddrTable),
    DDI_TABLE(zetTracerExp, zetGetTracerExpProcAddrTable),
    DDI_TABLE(zetCommandList, zetGetCommandListProcAddrTable),
    DDI_TABLE(zetModule, zetGetModuleProcAddrTable),
    DDI_TABLE(zetKernel, zetGetKernelProcAddrTable),
    DDI_TABLE(zetMetricGroup, zetGetMetricGroupProcAddrTable),
    DDI_TABLE(zetMetric, zetGetMetricProcAddrTable),
    DDI_TABLE(zetMetricQueryPool, zetGetMetricQueryPoolProcAddrTable),
    DDI_TABLE(zetMetricQuery, zetGetMetricQueryProcAddrTable),
    DDI_TABLE(zetDevice, zetGetDeviceProcAddrTable),
    DDI_TABLE(zetDebug, zetGetDebugProcAddrTable),
    DDI_TABLE(zetMetricGroupExp, zetGetMetricGroupExpProcAddrTable),
    DDI_TABLE(zetMetricProgrammableExp, zetGetMetricProgrammableExpProcAddrTable),
    DDI_TABLE(zetMetricExp, zetGetMetricExpProcAddrTable),
    DDI_TABLE(zetMetricTracerExp, zetGetMetricTracerExpProcAddrTable),
    DDI_TABLE(zetMetricDecoderExp, zetGetMetricDecoderExpProcAddrTable),
    DDI_TABLE(zetDeviceExp, zetGetDeviceExpProcAddrTable),
    DDI_TABLE(zetCommandListExp, zetGetCommandListExpProcAddrTable),
};

} // namespace ZetDdiTableManifest

constexpr size_t ddiTablesCount = std::size(ZetDdiTableManifest::ddiTables);
constexpr const DdiTableExpectation *ddiTables = ZetDdiTableManifest::ddiTables;

TEST(ZetDdiTableManifestTest, givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnce) {
    givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnceFunction(ddiTables, ddiTablesCount);
}

TEST(ZetDdiTableManifestTest, givenDdiTableManifestWhenComparingAgainstComponentVersionThenItMatchesHighestExposedApiVersion) {
    givenDdiTableManifestWhenComparingAgainstComponentVersionThenItMatchesHighestExposedApiVersionFunction(ddiTables, ddiTablesCount, globalDriverDispatch.tools.version);
}

TEST(ZetGetProcAddrTableTest, givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulated) {
    givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZetGetProcAddrTableTest, givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulated) {
    givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZetGetProcAddrTableTest, givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturned) {
    givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturnedFunction(ddiTables, ddiTablesCount);
}

} // namespace ult
} // namespace L0
