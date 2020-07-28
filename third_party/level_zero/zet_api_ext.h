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
/// @brief Forward-declare zet_metric_query_pool_desc_ext_t
typedef struct _zet_metric_query_pool_desc_ext_t zet_metric_query_pool_desc_ext_t;

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
/// @brief Metric query pool types
typedef enum _zet_metric_query_pool_type_t {
    ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE = 0, ///< Performance metric query pool.
    ZET_METRIC_QUERY_POOL_TYPE_EXECUTION = 1,   ///< Skips workload execution between begin/end calls.
    ZET_METRIC_QUERY_POOL_TYPE_FORCE_UINT32 = 0x7fffffff

} zet_metric_query_pool_type_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric query pool description
typedef struct _zet_metric_query_pool_desc_ext_t {
    zet_structure_type_t stype;        ///< [in] type of this structure
    const void *pNext;                 ///< [in][optional] pointer to extension-specific structure
    zet_metric_query_pool_type_t type; ///< [in] Query pool type.
    uint32_t count;                    ///< [in] Internal slots count within query pool object.

} zet_metric_query_pool_desc_ext_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Creates a pool of metric queries on the context.
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function must be thread-safe.
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
///         + `nullptr == phMetricQueryPool`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `::ZET_METRIC_QUERY_POOL_TYPE_EXECUTION < desc->type`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricQueryPoolCreateExt(
    zet_context_handle_t hContext,                    ///< [in] handle of the context object
    zet_device_handle_t hDevice,                      ///< [in] handle of the device
    zet_metric_group_handle_t hMetricGroup,           ///< [in] metric group associated with the query object.
    const zet_metric_query_pool_desc_ext_t *desc,     ///< [in] metric query pool descriptor
    zet_metric_query_pool_handle_t *phMetricQueryPool ///< [out] handle of metric query pool
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

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric group sampling type
typedef uint32_t zet_metric_group_sampling_type_flags_t;
typedef enum _zet_metric_group_sampling_type_flag_t {
    ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED = ZE_BIT(0), ///< Event based sampling
    ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED = ZE_BIT(1),  ///< Time based sampling
    ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_FORCE_UINT32 = 0x7fffffff

} zet_metric_group_sampling_type_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Metric group properties queried using ::zetMetricGroupGetProperties
typedef struct _zet_metric_group_properties_ext_t {
    zet_structure_type_t stype;                          ///< [in] type of this structure
    void *pNext;                                         ///< [in,out][optional] pointer to extension-specific structure
    char name[ZET_MAX_METRIC_GROUP_NAME];                ///< [out] metric group name
    char description[ZET_MAX_METRIC_GROUP_DESCRIPTION];  ///< [out] metric group description
    zet_metric_group_sampling_type_flags_t samplingType; ///< [out] metric group sampling type.
                                                         ///< returns a combination of ::zet_metric_group_sampling_type_flag_t.
    uint32_t domain;                                     ///< [out] metric group domain number. Cannot use simultaneous metric
                                                         ///< groups from different domains.
    uint32_t metricCount;                                ///< [out] metric count belonging to this group

} zet_metric_group_properties_ext_t;

/// @brief Metric properties queried using ::zetMetricGetProperties
typedef struct _zet_metric_properties_ext_t {
    zet_structure_type_t stype;                    ///< [in] type of this structure
    void *pNext;                                   ///< [in,out][optional] pointer to extension-specific structure
    char name[ZET_MAX_METRIC_NAME];                ///< [out] metric name
    char description[ZET_MAX_METRIC_DESCRIPTION];  ///< [out] metric description
    char component[ZET_MAX_METRIC_COMPONENT];      ///< [out] metric component
    uint32_t tierNumber;                           ///< [out] number of tier
    zet_metric_type_t metricType;                  ///< [out] metric type
    zet_value_type_t resultType;                   ///< [out] metric result type
    char resultUnits[ZET_MAX_METRIC_RESULT_UNITS]; ///< [out] metric result units

} zet_metric_properties_ext_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves attributes of a metric group.
///
/// @details
///     - The application may call this function from simultaneous threads.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMetricGroup`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pProperties`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGroupGetPropertiesExt(
    zet_metric_group_handle_t hMetricGroup,        ///< [in] handle of the metric group
    zet_metric_group_properties_ext_t *pProperties ///< [in,out] metric group properties
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieves attributes of a metric.
///
/// @details
///     - The application may call this function from simultaneous threads.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMetric`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pProperties`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetMetricGetPropertiesExt(
    zet_metric_handle_t hMetric,             ///< [in] handle of the metric
    zet_metric_properties_ext_t *pProperties ///< [in,out] metric properties
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Supportted profile features
typedef uint32_t zet_profile_flagsExt_t;
typedef enum _zet_profile_flagExt_t {
    ZET_PROFILE_FLAG_REGISTER_REALLOCATION_EXT = ZE_BIT(0), ///< request the compiler attempt to minimize register usage as much as
                                                            ///< possible to allow for instrumentation
    ZET_PROFILE_FLAG_FREE_REGISTER_INFO_EXT = ZE_BIT(1),    ///< request the compiler generate free register info
    ZET_PROFILE_FLAG_FORCE_UINT32_EXT = 0x7fffffff

} zet_profile_flagExt_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Profiling meta-data for instrumentation
typedef struct _zet_profile_properties_t {
    zet_structure_type_t stype;   ///< [in] type of this structure
    void *pNext;                  ///< [in,out][optional] pointer to extension-specific structure
    zet_profile_flagsExt_t flags; ///< [out] indicates which flags were enabled during compilation.
                                  ///< returns 0 (none) or a combination of ::zet_profile_flag_t
    uint32_t numTokens;           ///< [out] number of tokens immediately following this structure

} zet_profile_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Retrieve profiling information generated for the kernel.
///
/// @details
///     - Module must be created using the following build option:
///         + "-zet-profile-flags <n>" - enable generation of profile
///           information
///         + "<n>" must be a combination of ::zet_profile_flag_t, in hex
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hKernel`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pProfileProperties`
ZE_APIEXPORT ze_result_t ZE_APICALL
zetKernelGetProfileInfoExt(
    zet_kernel_handle_t hKernel,                 ///< [in] handle to kernel
    zet_profile_properties_t *pProfileProperties ///< [out] pointer to profile properties
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZET_API_EXT_H
