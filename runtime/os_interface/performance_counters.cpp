/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/performance_counters.h"

#include "runtime/utilities/tag_allocator.h"

using namespace MetricsLibraryApi;

namespace NEO {

//////////////////////////////////////////////////////
// PerformanceCounters constructor.
//////////////////////////////////////////////////////
PerformanceCounters::PerformanceCounters() {
    metricsLibrary = std::make_unique<MetricsLibrary>();
    UNRECOVERABLE_IF(metricsLibrary == nullptr);
}

//////////////////////////////////////////////////////
// PerformanceCounters::getReferenceNumber
//////////////////////////////////////////////////////
uint32_t PerformanceCounters::getReferenceNumber() {
    std::lock_guard<std::mutex> lockMutex(mutex);
    return referenceCounter;
}

//////////////////////////////////////////////////////
// PerformanceCounters::isAvailable
//////////////////////////////////////////////////////
bool PerformanceCounters::isAvailable() {
    return available;
}

//////////////////////////////////////////////////////
// PerformanceCounters::enable
//////////////////////////////////////////////////////
void PerformanceCounters::enable() {
    std::lock_guard<std::mutex> lockMutex(mutex);

    if (referenceCounter == 0) {
        available = openMetricsLibrary();
    }

    referenceCounter++;
}

//////////////////////////////////////////////////////
// PerformanceCounters::shutdown
//////////////////////////////////////////////////////
void PerformanceCounters::shutdown() {
    std::lock_guard<std::mutex> lockMutex(mutex);

    if (referenceCounter >= 1) {
        if (referenceCounter == 1) {
            available = false;
            closeMetricsLibrary();
        }
        referenceCounter--;
    }
}

//////////////////////////////////////////////////////
// PerformanceCounters::getMetricsLibraryInterface
//////////////////////////////////////////////////////
MetricsLibrary *PerformanceCounters::getMetricsLibraryInterface() {
    return metricsLibrary.get();
}

//////////////////////////////////////////////////////
// PerformanceCounters::setMetricsLibraryInterface
//////////////////////////////////////////////////////
void PerformanceCounters::setMetricsLibraryInterface(std::unique_ptr<MetricsLibrary> newMetricsLibrary) {
    metricsLibrary = std::move(newMetricsLibrary);
}

//////////////////////////////////////////////////////
// PerformanceCounters::getMetricsLibraryContext
//////////////////////////////////////////////////////
ContextHandle_1_0 PerformanceCounters::getMetricsLibraryContext() {
    return context;
}

//////////////////////////////////////////////////////
// PerformanceCounters::openMetricsLibrary
//////////////////////////////////////////////////////
bool PerformanceCounters::openMetricsLibrary() {

    // Open metrics library.
    bool result = metricsLibrary->open();
    DEBUG_BREAK_IF(!result);

    // Create metrics library context.
    if (result) {
        result = metricsLibrary->contextCreate(
            clientType,
            clientData,
            contextData,
            context);

        // Validate gpu report size.
        DEBUG_BREAK_IF(!metricsLibrary->hwCountersGetGpuReportSize());
    }

    // Error handling.
    if (!result) {
        closeMetricsLibrary();
    }

    return result;
}

//////////////////////////////////////////////////////
// PerformanceCounters::closeMetricsLibrary
//////////////////////////////////////////////////////
void PerformanceCounters::closeMetricsLibrary() {
    // Destroy oa/user mmio configuration.
    releaseCountersConfiguration();

    // Destroy hw counters query.
    if (query.IsValid()) {
        metricsLibrary->hwCountersDelete(query);
    }

    // Destroy metrics library context.
    if (context.IsValid()) {
        metricsLibrary->contextDelete(context);
    }
}

//////////////////////////////////////////////////////
// PerformanceCounters::getQueryHandle
//////////////////////////////////////////////////////
QueryHandle_1_0 PerformanceCounters::getQueryHandle() {
    if (!query.IsValid()) {
        metricsLibrary->hwCountersCreate(
            context,
            1,
            userConfiguration,
            query);
    }

    DEBUG_BREAK_IF(!query.IsValid());
    return query;
}

//////////////////////////////////////////////////////
// PerformanceCounters::getGpuCommandsSize
//////////////////////////////////////////////////////
uint32_t PerformanceCounters::getGpuCommandsSize(
    const MetricsLibraryApi::GpuCommandBufferType commandBufferType,
    const bool begin) {
    CommandBufferData_1_0 bufferData = {};
    CommandBufferSize_1_0 bufferSize = {};

    if (begin) {
        // Load currently activated (through metrics discovery) oa/user mmio configuration and use it.
        // It will allow to change counters configuration between subsequent clEnqueueNDCommandRange calls.
        if (!enableCountersConfiguration()) {
            return 0;
        }
    }

    bufferData.HandleContext = context;
    bufferData.CommandsType = ObjectType::QueryHwCounters;
    bufferData.Type = commandBufferType;

    bufferData.QueryHwCounters.Begin = begin;
    bufferData.QueryHwCounters.Handle = getQueryHandle();
    bufferData.QueryHwCounters.HandleUserConfiguration = userConfiguration;

    return metricsLibrary->commandBufferGetSize(bufferData, bufferSize)
               ? bufferSize.GpuMemorySize
               : 0;
}

//////////////////////////////////////////////////////
// PerformanceCounters::getGpuCommands
//////////////////////////////////////////////////////
bool PerformanceCounters::getGpuCommands(
    const MetricsLibraryApi::GpuCommandBufferType commandBufferType,
    TagNode<HwPerfCounter> &performanceCounters,
    const bool begin,
    const uint32_t bufferSize,
    void *pBuffer) {

    // Command Buffer data.
    CommandBufferData_1_0 bufferData = {};
    bufferData.HandleContext = context;
    bufferData.CommandsType = ObjectType::QueryHwCounters;
    bufferData.Data = pBuffer;
    bufferData.Size = bufferSize;
    bufferData.Type = commandBufferType;

    // Gpu memory allocation for query hw counters.
    bufferData.Allocation.CpuAddress = reinterpret_cast<uint8_t *>(performanceCounters.tagForCpuAccess);
    bufferData.Allocation.GpuAddress = performanceCounters.getGpuAddress();

    // Query hw counters specific data.
    bufferData.QueryHwCounters.Begin = begin;
    bufferData.QueryHwCounters.Handle = getQueryHandle();
    bufferData.QueryHwCounters.HandleUserConfiguration = userConfiguration;

    return metricsLibrary->commandBufferGet(bufferData);
}

//////////////////////////////////////////////////////
// PerformanceCounters::getApiReportSize
//////////////////////////////////////////////////////
uint32_t PerformanceCounters::getApiReportSize() {
    return metricsLibrary->hwCountersGetApiReportSize();
}

//////////////////////////////////////////////////////
// PerformanceCounters::getGpuReportSize
//////////////////////////////////////////////////////
uint32_t PerformanceCounters::getGpuReportSize() {
    return metricsLibrary->hwCountersGetGpuReportSize();
}

//////////////////////////////////////////////////////
// PerformanceCounters::getApiReport
//////////////////////////////////////////////////////
bool PerformanceCounters::getApiReport(const size_t inputParamSize, void *pInputParam, size_t *pOutputParamSize, bool isEventComplete) {
    const uint32_t outputSize = metricsLibrary->hwCountersGetApiReportSize();

    if (pOutputParamSize) {
        *pOutputParamSize = outputSize;
    }

    if (pInputParam == nullptr && inputParamSize == 0 && pOutputParamSize) {
        return true;
    }

    if (pInputParam == nullptr || isEventComplete == false) {
        return false;
    }

    if (inputParamSize < outputSize) {
        return false;
    }

    return metricsLibrary->hwCountersGetReport(query, 0, 1, outputSize, pInputParam);
}
} // namespace NEO
