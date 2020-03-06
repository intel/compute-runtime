/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/device.h"
#include "level_zero/tools/source/metrics/metric.h"
#include <level_zero/zet_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zetMetricGroupGet(
    zet_device_handle_t hDevice,
    uint32_t *pCount,
    zet_metric_group_handle_t *phMetricGroups) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::metricGroupGet(hDevice, pCount, phMetricGroups);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricGroupGetProperties(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_properties_t *pProperties) {
    try {
        {
            if (nullptr == hMetricGroup)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricGroup::fromHandle(hMetricGroup)->getProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricGet(
    zet_metric_group_handle_t hMetricGroup,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    try {
        {
            if (nullptr == hMetricGroup)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::metricGet(hMetricGroup, pCount, phMetrics);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricGetProperties(
    zet_metric_handle_t hMetric,
    zet_metric_properties_t *pProperties) {
    try {
        {
            if (nullptr == hMetric)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pProperties)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Metric::fromHandle(hMetric)->getProperties(pProperties);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricGroupCalculateMetricValues(
    zet_metric_group_handle_t hMetricGroup,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pMetricValueCount,
    zet_typed_value_t *pMetricValues) {
    try {
        {
            if (nullptr == hMetricGroup)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRawData)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pMetricValueCount)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricGroup::fromHandle(hMetricGroup)->calculateMetricValues(rawDataSize, pRawData, pMetricValueCount, pMetricValues);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetDeviceActivateMetricGroups(
    zet_device_handle_t hDevice,
    uint32_t count,
    zet_metric_group_handle_t *phMetricGroups) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->activateMetricGroups(count, phMetricGroups);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricTracerOpen(
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_tracer_desc_t *pDesc,
    ze_event_handle_t hNotificationEvent,
    zet_metric_tracer_handle_t *phMetricTracer) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hMetricGroup)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pDesc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phMetricTracer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZET_METRIC_TRACER_DESC_VERSION_CURRENT < pDesc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::metricTracerOpen(hDevice, hMetricGroup, pDesc, hNotificationEvent, phMetricTracer);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricTracerMarker(
    ze_command_list_handle_t hCommandList,
    zet_metric_tracer_handle_t hMetricTracer,
    uint32_t value) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hMetricTracer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMetricTracerMarker(hMetricTracer, value);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricTracerClose(
    zet_metric_tracer_handle_t hMetricTracer) {
    try {
        {
            if (nullptr == hMetricTracer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricTracer::fromHandle(hMetricTracer)->close();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricTracerReadData(
    zet_metric_tracer_handle_t hMetricTracer,
    uint32_t maxReportCount,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    try {
        {
            if (nullptr == hMetricTracer)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRawDataSize)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricTracer::fromHandle(hMetricTracer)->readData(maxReportCount, pRawDataSize, pRawData);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryPoolCreate(
    zet_device_handle_t hDevice,
    zet_metric_group_handle_t hMetricGroup,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phMetricQueryPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZET_METRIC_QUERY_POOL_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::metricQueryPoolCreate(hDevice, hMetricGroup, desc, phMetricQueryPool);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryPoolDestroy(
    zet_metric_query_pool_handle_t hMetricQueryPool) {
    try {
        {
            if (nullptr == hMetricQueryPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::metricQueryPoolDestroy(hMetricQueryPool);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryCreate(
    zet_metric_query_pool_handle_t hMetricQueryPool,
    uint32_t index,
    zet_metric_query_handle_t *phMetricQuery) {
    try {
        {
            if (nullptr == hMetricQueryPool)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricQueryPool::fromHandle(hMetricQueryPool)->createMetricQuery(index, phMetricQuery);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryDestroy(
    zet_metric_query_handle_t hMetricQuery) {
    try {
        {
            if (nullptr == hMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricQuery::fromHandle(hMetricQuery)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryReset(
    zet_metric_query_handle_t hMetricQuery) {
    try {
        {
            if (nullptr == hMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricQuery::fromHandle(hMetricQuery)->reset();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricQueryBegin(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryBegin(hMetricQuery);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricQueryEnd(
    zet_command_list_handle_t hCommandList,
    zet_metric_query_handle_t hMetricQuery,
    ze_event_handle_t hCompletionEvent) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == hMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMetricQueryEnd(hMetricQuery, hCompletionEvent);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetCommandListAppendMetricMemoryBarrier(
    zet_command_list_handle_t hCommandList) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMetricMemoryBarrier();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zetMetricQueryGetData(
    zet_metric_query_handle_t hMetricQuery,
    size_t *pRawDataSize,
    uint8_t *pRawData) {
    try {
        {
            if (nullptr == hMetricQuery)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRawDataSize)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::MetricQuery::fromHandle(hMetricQuery)->getData(pRawDataSize, pRawData);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
