/*
 *
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 *
 */
#ifndef _ZET_API_EXT_H
#define _ZET_API_EXT_H
#if defined(__cplusplus)
#pragma once
#endif

// standard headers
#include <level_zero/zet_api.h>

#include "third_party/level_zero/ze_api_ext.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

// Intel 'oneAPI' Level-Zero Tool API common types
///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of context object
typedef ze_context_handle_t zet_context_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle of metric streamer's object
typedef struct _zet_metric_streamer_handle_t *zet_metric_streamer_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Defines structure types
typedef enum _zet_structure_type_t {
    ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES = 0x1, ///< ::zet_metric_group_properties_t
    ZET_STRUCTURE_TYPE_METRIC_PROPERTIES = 0x2,       ///< ::zet_metric_properties_t
    ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC = 0x3,    ///< ::zet_metric_streamer_desc_t
    ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC = 0x4,  ///< ::zet_metric_query_pool_desc_t
    ZET_STRUCTURE_TYPE_PROFILE_PROPERTIES = 0x5,      ///< ::zet_profile_properties_t
    ZET_STRUCTURE_TYPE_DEVICE_DEBUG_PROPERTIES = 0x6, ///< ::zet_device_debug_properties_t
    ZET_STRUCTURE_TYPE_DEBUG_MEMORY_SPACE_DESC = 0x7, ///< ::zet_debug_memory_space_desc_t
    ZET_STRUCTURE_TYPE_DEBUG_REGSET_PROPERTIES = 0x8, ///< ::zet_debug_regset_properties_t
    ZET_STRUCTURE_TYPE_TRACER_EXP_DESC = 0x00010001,  ///< ::zet_tracer_exp_desc_t
    ZET_STRUCTURE_TYPE_FORCE_UINT32 = 0x7fffffff

} zet_structure_type_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Forward-declare zet_metric_streamer_desc_t
typedef struct _zet_metric_streamer_desc_t zet_metric_streamer_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric streamer descriptor
typedef struct _zet_metric_streamer_desc_t {
    zet_structure_type_t stype;   ///< [in] type of this structure
    const void *pNext;            ///< [in][optional] pointer to extension-specific structure
    uint32_t notifyEveryNReports; ///< [in,out] number of collected reports after which notification event
                                  ///< will be signalled
    uint32_t samplingPeriod;      ///< [in,out] streamer sampling period in nanoseconds

} zet_metric_streamer_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Opens metric streamer for a device.
///
/// @details
///     - The notification event must have been created from an event pool that
///       was created using ::ZE_EVENT_POOL_FLAG_HOST_VISIBLE flag.
///     - The notification event must **not** have been created from an event
///       pool that was created using ::ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP
///       flag.
///     - The application must **not** call this function from simultaneous
///       threads with the same device handle.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///         + `nullptr == hMetricGroup`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == desc`
///         + `nullptr == phMetricStreamer`
///     - ::ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerOpen(
    zet_device_handle_t hDevice,                   ///< [in] handle of the device
    zet_metric_group_handle_t hMetricGroup,        ///< [in] handle of the metric group
    zet_metric_streamer_desc_t *desc,              ///< [in,out] metric streamer descriptor
    ze_event_handle_t hNotificationEvent,          ///< [in][optional] event used for report availability notification
    zet_metric_streamer_handle_t *phMetricStreamer ///< [out] handle of metric streamer
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Append metric streamer marker into a command list.
///
/// @details
///     - The application must ensure the metric streamer is accessible by the
///       device on which the command list was created.
///     - The application must ensure the command list and metric streamer were
///       created on the same context.
///     - The application must **not** call this function from simultaneous
///       threads with the same command list handle.
///     - Allow to associate metric stream time based metrics with executed
///       workload.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hCommandList`
///         + `nullptr == hMetricStreamer`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricStreamerMarker(
    zet_command_list_handle_t hCommandList,       ///< [in] handle of the command list
    zet_metric_streamer_handle_t hMetricStreamer, ///< [in] handle of the metric streamer
    uint32_t value                                ///< [in] streamer marker value
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Closes metric streamer.
///
/// @details
///     - The application must **not** call this function from simultaneous
///       threads with the same metric streamer handle.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMetricStreamer`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerClose(
    zet_metric_streamer_handle_t hMetricStreamer ///< [in][release] handle of the metric streamer
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Reads data from metric streamer.
///
/// @details
///     - The application may call this function from simultaneous threads.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMetricStreamer`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pRawDataSize`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricStreamerReadData(
    zet_metric_streamer_handle_t hMetricStreamer, ///< [in] handle of the metric streamer
    uint32_t maxReportCount,                      ///< [in] the maximum number of reports the application wants to receive.
                                                  ///< if UINT32_MAX, then function will retrieve all reports available
    size_t *pRawDataSize,                         ///< [in,out] pointer to size in bytes of raw data requested to read.
                                                  ///< if size is zero, then the driver will update the value with the total
                                                  ///< size in bytes needed for all reports available.
                                                  ///< if size is non-zero, then driver will only retrieve the number of
                                                  ///< reports that fit into the buffer.
                                                  ///< if size is larger than size needed for all reports, then driver will
                                                  ///< update the value with the actual size needed.
    uint8_t *pRawData                             ///< [in,out][optional][range(0, *pRawDataSize)] buffer containing streamer
                                                  ///< reports in raw format
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Activates metric groups.
///
/// @details
///     - Immediately reconfigures the device to activate only those metric
///       groups provided.
///     - Any metric groups previously activated but not provided will be
///       deactivated.
///     - Deactivating metric groups that are still in-use will result in
///       undefined behavior.
///     - All metric groups must have different domains, see
///       ::zet_metric_group_properties_t.
///     - The application must **not** call this function from simultaneous
///       threads with the same device handle.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hContext`
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_SIZE
///         + `(nullptr == phMetricGroups) && (0 < count)`
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + Multiple metric groups share the same domain
ZE_APIEXPORT ze_result_t ZE_APICALL
zetContextActivateMetricGroups(
    zet_context_handle_t hContext,            ///< [in] handle of the context object
    zet_device_handle_t hDevice,              ///< [in] handle of the device
    uint32_t count,                           ///< [in] metric group count to activate; must be 0 if `nullptr ==
                                              ///< phMetricGroups`
    zet_metric_group_handle_t *phMetricGroups ///< [in][optional][range(0, count)] handles of the metric groups to activate.
                                              ///< nullptr deactivates all previously used metric groups.
                                              ///< all metrics groups must come from a different domains.
                                              ///< metric query and metric stream must use activated metric groups.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Appends metric query end into a command list.
///
/// @details
///     - The application must ensure the metric query and events are accessible
///       by the device on which the command list was created.
///     - The application must ensure the command list, events and metric query
///       were created on the same context.
///     - The application must ensure the signal event was **not** created from
///       an event pool that was created using
///       ::ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP flag.
///     - The application must **not** call this function from simultaneous
///       threads with the same command list handle.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hCommandList`
///         + `nullptr == hMetricQuery`
///     - ::ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT
///     - ::ZE_RESULT_ERROR_INVALID_SIZE
///         + `(nullptr == phWaitEvents) && (0 < numWaitEvents)`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetCommandListAppendMetricQueryEndExt(
    zet_command_list_handle_t hCommandList, ///< [in] handle of the command list
    zet_metric_query_handle_t hMetricQuery, ///< [in] handle of the metric query
    ze_event_handle_t hSignalEvent,         ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                 ///< [in][optional] number of events to wait on before launching; must be 0
                                            ///< if `nullptr == phWaitEvents`
    ze_event_handle_t *phWaitEvents         ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait
                                            ///< on before launching
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZET_API_EXT_H
