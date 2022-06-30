/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"

namespace L0 {

OaMetricSourceImp::OsLibraryLoadPtr OaMetricSourceImp::osLibraryLoadFunction(NEO::OsLibrary::load);

std::unique_ptr<OaMetricSourceImp> OaMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<OaMetricSourceImp>(new (std::nothrow) OaMetricSourceImp(metricDeviceContext));
}

OaMetricSourceImp::OaMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext),
                                                                                       metricEnumeration(std::unique_ptr<MetricEnumeration>(new (std::nothrow) MetricEnumeration(*this))),
                                                                                       metricsLibrary(std::unique_ptr<MetricsLibrary>(new (std::nothrow) MetricsLibrary(*this))) {
}

OaMetricSourceImp::~OaMetricSourceImp() = default;

void OaMetricSourceImp::enable() {
    loadDependencies();
}

bool OaMetricSourceImp::isAvailable() {
    return isInitialized();
}

ze_result_t OaMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    DeviceImp *pDeviceImp = static_cast<DeviceImp *>(commandList.device);

    if (pDeviceImp->metricContext->isImplicitScalingCapable()) {
        // Use one of the sub-device contexts to append to command list.
        pDeviceImp = static_cast<DeviceImp *>(pDeviceImp->subDevices[0]);
    }

    auto &metricContext = pDeviceImp->getMetricDeviceContext();
    auto &metricsLibrary = metricContext.getMetricSource<OaMetricSourceImp>().getMetricsLibrary();

    // Obtain gpu commands.
    CommandBufferData_1_0 commandBuffer = {};
    commandBuffer.CommandsType = MetricsLibraryApi::ObjectType::OverrideFlushCaches;
    commandBuffer.Override.Enable = true;
    commandBuffer.Type = metricContext.getMetricSource<OaMetricSourceImp>().isComputeUsed()
                             ? MetricsLibraryApi::GpuCommandBufferType::Compute
                             : MetricsLibraryApi::GpuCommandBufferType::Render;

    return metricsLibrary.getGpuCommands(commandList, commandBuffer) ? ZE_RESULT_SUCCESS
                                                                     : ZE_RESULT_ERROR_UNKNOWN;
}

bool OaMetricSourceImp::loadDependencies() {
    bool result = true;
    if (metricEnumeration->loadMetricsDiscovery() != ZE_RESULT_SUCCESS) {
        result = false;
        DEBUG_BREAK_IF(!result);
    }
    if (result && !metricsLibrary->load()) {
        result = false;
        DEBUG_BREAK_IF(!result);
    }

    // Set metric context initialization state.
    setInitializationState(result
                               ? ZE_RESULT_SUCCESS
                               : ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    return result;
}
bool OaMetricSourceImp::isInitialized() {
    return initializationState == ZE_RESULT_SUCCESS;
}

void OaMetricSourceImp::setInitializationState(const ze_result_t state) {
    initializationState = state;
}

Device &OaMetricSourceImp::getDevice() {
    return metricDeviceContext.getDevice();
}

MetricsLibrary &OaMetricSourceImp::getMetricsLibrary() {
    return *metricsLibrary;
}
MetricEnumeration &OaMetricSourceImp::getMetricEnumeration() {
    return *metricEnumeration;
}
MetricStreamer *OaMetricSourceImp::getMetricStreamer() {
    return pMetricStreamer;
}

void OaMetricSourceImp::setMetricStreamer(MetricStreamer *pMetricStreamer) {
    this->pMetricStreamer = pMetricStreamer;
}

void OaMetricSourceImp::setMetricsLibrary(MetricsLibrary &metricsLibrary) {
    this->metricsLibrary.release();
    this->metricsLibrary.reset(&metricsLibrary);
}

void OaMetricSourceImp::setMetricEnumeration(MetricEnumeration &metricEnumeration) {
    this->metricEnumeration.release();
    this->metricEnumeration.reset(&metricEnumeration);
}

void OaMetricSourceImp::setUseCompute(const bool useCompute) {
    this->useCompute = useCompute;
}

bool OaMetricSourceImp::isComputeUsed() const {
    return useCompute;
}

ze_result_t OaMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    return getMetricEnumeration().metricGroupGet(*pCount, phMetricGroups);
}

uint32_t OaMetricSourceImp::getSubDeviceIndex() {
    return metricDeviceContext.getSubDeviceIndex();
}

bool OaMetricSourceImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    return metricDeviceContext.isMetricGroupActivated(hMetricGroup);
}

bool OaMetricSourceImp::isMetricGroupActivated() const {
    return metricDeviceContext.isMetricGroupActivated();
}

bool OaMetricSourceImp::isImplicitScalingCapable() const {
    return metricDeviceContext.isImplicitScalingCapable();
}

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const {
    return static_cast<OaMetricSourceImp &>(*metricSources.at(MetricSource::SourceType::Oa));
}

} // namespace L0
