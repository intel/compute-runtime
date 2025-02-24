/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZET_INTEL_GPU_METRIC_H
#define _ZET_INTEL_GPU_METRIC_H

#include "level_zero/include/level_zero/ze_stypes.h"
#include <level_zero/zet_api.h>

#if defined(__cplusplus)
#pragma once
extern "C" {
#endif

#include <stdint.h>

#define ZET_INTEL_GPU_METRIC_VERSION_MAJOR 0
#define ZET_INTEL_GPU_METRIC_VERSION_MINOR 2
#define ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP 64u
#define ZET_INTEL_METRIC_PROGRAMMABLE_PARAM_TYPE_GENERIC_EXP (0x7ffffffe)

#ifndef ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME
/// @brief Extension name for query to read the Intel Level Zero Driver Version String
#define ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME "ZET_intel_metric_source_id"
#endif // ZET_INTEL_METRIC_SOURCE_ID_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric Source Id extension Version(s)
typedef enum _zet_intel_metric_source_id_exp_version_t {
    ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZET_INTEL_METRIC_SOURCE_ID_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_source_id_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Query an unique identifier representing the source of a metric group
/// This structure can be passed in the 'pNext' of zet_metric_group_properties_t
typedef struct _zet_intel_metric_source_id_exp_t {
    zet_structure_type_t stype; ///< [in] type of this structure
    const void *pNext;          ///< [in][optional] must be null or a pointer to an extension-specific
                                ///< structure (i.e. contains stype and pNext).
    uint32_t sourceId;          ///< [out] Returns an unique source Id of the metric group
} zet_intel_metric_source_id_exp_t;

#ifndef ZET_INTEL_METRIC_APPEND_MARKER_EXP_NAME
/// @brief Extension name for query to read the Intel Level Zero Driver Version String
#define ZET_INTEL_METRIC_APPEND_MARKER_EXP_NAME "ZET_intel_metric_append_marker"
#endif // ZET_INTEL_APPEND_MARKER_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Append Marker extension Version(s)
typedef enum _zet_intel_metric_append_marker_exp_version_t {
    ZET_INTEL_METRIC_APPEND_MARKER_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZET_INTEL_METRIC_APPEND_MARKER_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZET_INTEL_METRIC_APPEND_MARKER_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_append_marker_exp_version_t;

#define ZET_INTEL_METRIC_GROUP_TYPE_EXP_FLAG_MARKER (static_cast<zet_metric_group_type_exp_flag_t>(ZE_BIT(3))) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901

///////////////////////////////////////////////////////////////////////////////
/// @brief Appends a metric marker to the command list based on the metricGroup
/// Metrics can be generated by multiple sources in the HW
/// This API generates a marker through the same source as the metric group
ze_result_t ZE_APICALL zetIntelCommandListAppendMarkerExp(zet_command_list_handle_t hCommandList,
                                                          zet_metric_group_handle_t hMetricGroup,
                                                          uint32_t value);

typedef zet_metric_tracer_exp_desc_t zet_intel_metric_tracer_exp_desc_t;
typedef zet_metric_tracer_exp_handle_t zet_intel_metric_tracer_exp_handle_t;
typedef zet_metric_decoder_exp_handle_t zet_intel_metric_decoder_exp_handle_t;
typedef zet_metric_entry_exp_t zet_intel_metric_entry_exp_t;

ze_result_t ZE_APICALL zetIntelMetricTracerCreateExp(
    zet_context_handle_t hContext,                       ///< [in] handle of the context object
    zet_device_handle_t hDevice,                         ///< [in] handle of the device
    uint32_t metricGroupCount,                           ///< [in] metric group count
    zet_metric_group_handle_t *phMetricGroups,           ///< [in][range(0, metricGroupCount )] handles of the metric groups to
                                                         ///< trace
    zet_intel_metric_tracer_exp_desc_t *desc,            ///< [in,out] metric tracer descriptor
    ze_event_handle_t hNotificationEvent,                ///< [in][optional] event used for report availability notification. Note:
                                                         ///< If buffer is not drained when the event it flagged, there is a risk of
                                                         ///< HW event buffer being overrun
    zet_intel_metric_tracer_exp_handle_t *phMetricTracer ///< [out] handle of the metric tracer
);

ze_result_t ZE_APICALL zetIntelMetricTracerDestroyExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer ///< [in] handle of the metric tracer
);

ze_result_t ZE_APICALL zetIntelMetricTracerEnableExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer, ///< [in] handle of the metric tracer
    ze_bool_t synchronous                               ///< [in] request synchronous behavior. Confirmation of successful
                                                        ///< asynchronous operation is done by calling ::zetMetricTracerReadDataExp()
                                                        ///< and checking the return status: ::ZE_RESULT_NOT_READY will be returned
                                                        ///< when the tracer is inactive. ::ZE_RESULT_SUCCESS will be returned
                                                        ///< when the tracer is active.
);

ze_result_t ZE_APICALL zetIntelMetricTracerDisableExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer, ///< [in] handle of the metric tracer
    ze_bool_t synchronous                               ///< [in] request synchronous behavior. Confirmation of successful
                                                        ///< asynchronous operation is done by calling ::zetMetricTracerReadDataExp()
                                                        ///< and checking the return status: ::ZE_RESULT_SUCCESS will be returned
                                                        ///< when the tracer is active or when it is inactive but still has data.
                                                        ///< ::ZE_RESULT_NOT_READY will be returned when the tracer is inactive and
                                                        ///< has no more data to be retrieved.
);

ze_result_t ZE_APICALL zetIntelMetricTracerReadDataExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer, ///< [in] handle of the metric tracer
    size_t *pRawDataSize,                               ///< [in,out] pointer to size in bytes of raw data requested to read.
                                                        ///< if size is zero, then the driver will update the value with the total
                                                        ///< size in bytes needed for all data available.
                                                        ///< if size is non-zero, then driver will only retrieve that amount of
                                                        ///< data.
                                                        ///< if size is larger than size needed for all data, then driver will
                                                        ///< update the value with the actual size needed.
    uint8_t *pRawData                                   ///< [in,out][optional][range(0, *pRawDataSize)] buffer containing tracer
                                                        ///< data in raw format
);

ze_result_t ZE_APICALL zetIntelMetricDecoderCreateExp(
    zet_intel_metric_tracer_exp_handle_t hMetricTracer,    ///< [in] handle of the metric tracer
    zet_intel_metric_decoder_exp_handle_t *phMetricDecoder ///< [out] handle of the metric decoder object
);

ze_result_t ZE_APICALL zetIntelMetricDecoderDestroyExp(
    zet_intel_metric_decoder_exp_handle_t phMetricDecoder ///< [in] handle of the metric decoder object
);

ze_result_t ZE_APICALL zetIntelMetricDecoderGetDecodableMetricsExp(
    zet_intel_metric_decoder_exp_handle_t hMetricDecoder, ///< [in] handle of the metric decoder object
    uint32_t *pCount,                                     ///< [in,out] pointer to number of decodable metric in the hMetricDecoder
                                                          ///< handle. If count is zero, then the driver shall
                                                          ///< update the value with the total number of decodable metrics available
                                                          ///< in the decoder. if count is greater than zero
                                                          ///< but less than the total number of decodable metrics available in the
                                                          ///< decoder, then only that number will be returned.
                                                          ///< if count is greater than the number of decodable metrics available in
                                                          ///< the decoder, then the driver shall update the
                                                          ///< value with the actual number of decodable metrics available.
    zet_metric_handle_t *phMetrics                        ///< [in,out] [range(0, *pCount)] array of handles of decodable metrics in
                                                          ///< the hMetricDecoder handle provided.
);

ze_result_t ZE_APICALL zetIntelMetricTracerDecodeExp(
    zet_intel_metric_decoder_exp_handle_t phMetricDecoder, ///< [in] handle of the metric decoder object
    size_t *pRawDataSize,                                  ///< [in,out] size in bytes of raw data buffer. If pMetricEntriesCount is
                                                           ///< greater than zero but less than total number of
                                                           ///< decodable metrics available in the raw data buffer, then driver shall
                                                           ///< update this value with actual number of raw
                                                           ///< data bytes processed.
    uint8_t *pRawData,                                     ///< [in,out][optional][range(0, *pRawDataSize)] buffer containing tracer
                                                           ///< data in raw format
    uint32_t metricsCount,                                 ///< [in] number of decodable metrics in the tracer for which the
                                                           ///< hMetricDecoder handle was provided. See
                                                           ///< ::zetMetricDecoderGetDecodableMetricsExp(). If metricCount is greater
                                                           ///< than zero but less than the number decodable
                                                           ///< metrics available in the raw data buffer, then driver shall only
                                                           ///< decode those.
    zet_metric_handle_t *phMetrics,                        ///< [in] [range(0, metricsCount)] array of handles of decodable metrics in
                                                           ///< the decoder for which the hMetricDecoder handle was
                                                           ///< provided. Metrics handles are expected to be for decodable metrics,
                                                           ///< see ::zetMetricDecoderGetDecodableMetricsExp()
    uint32_t *pSetCount,                                   ///< [in,out] pointer to number of metric sets. If count is zero, then the
                                                           ///< driver shall update the value with the total
                                                           ///< number of metric sets to be decoded. If count is greater than the
                                                           ///< number available in the raw data buffer, then the
                                                           ///< driver shall update the value with the actual number of metric sets to
                                                           ///< be decoded. There is a 1:1 relation between
                                                           ///< the number of sets and sub-devices returned in the decoded entries.
    uint32_t *pMetricEntriesCountPerSet,                   ///< [in,out][optional][range(0, *pSetCount)] buffer of metric entries
                                                           ///< counts per metric set, one value per set.
    uint32_t *pMetricEntriesCount,                         ///< [in,out]  pointer to the total number of metric entries decoded, for
                                                           ///< all metric sets. If count is zero, then the
                                                           ///< driver shall update the value with the total number of metric entries
                                                           ///< to be decoded. If count is greater than zero
                                                           ///< but less than the total number of metric entries available in the raw
                                                           ///< data, then user provided number will be decoded.
                                                           ///< If count is greater than the number available in the raw data buffer,
                                                           ///< then the driver shall update the value with
                                                           ///< the actual number of decodable metric entries decoded. If set to null,
                                                           ///< then driver will only update the value of
                                                           ///< pSetCount.
    zet_intel_metric_entry_exp_t *pMetricEntries           ///< [in,out][optional][range(0, *pMetricEntriesCount)] buffer containing
                                                           ///< decoded metric entries
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif
