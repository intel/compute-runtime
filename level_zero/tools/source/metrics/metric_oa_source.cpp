/*
 * Copyright (C) 2022-2024 Intel Corporation
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
                                                                                       metricEnumeration(std::unique_ptr<MetricEnumeration>(new(std::nothrow) MetricEnumeration(*this))),
                                                                                       metricsLibrary(std::unique_ptr<MetricsLibrary>(new(std::nothrow) MetricsLibrary(*this))) {
    metricOAOsInterface = MetricOAOsInterface::create(metricDeviceContext.getDevice());
    activationTracker = std::make_unique<MultiDomainDeferredActivationTracker>(metricDeviceContext.getSubDeviceIndex());
    type = MetricSource::metricSourceTypeOa;
}

OaMetricSourceImp::~OaMetricSourceImp() = default;

void OaMetricSourceImp::enable() {
    loadDependencies();
}

ze_result_t OaMetricSourceImp::getTimerResolution(uint64_t &resolution) {

    ze_result_t result = getMetricOsInterface()->getMetricsTimerResolution(resolution);
    if (result != ZE_RESULT_SUCCESS) {
        resolution = 0;
    }

    return result;
}

ze_result_t OaMetricSourceImp::getTimestampValidBits(uint64_t &validBits) {
    ze_result_t retVal = ZE_RESULT_SUCCESS;

    uint64_t maxNanoSeconds = 0;
    if (!metricEnumeration->readGlobalSymbol(globalSymbolOaMaxTimestamp.data(), maxNanoSeconds)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t timerFreqquency;
    retVal = getTimerResolution(timerFreqquency);
    if (retVal != ZE_RESULT_SUCCESS) {
        validBits = 0;
        return retVal;
    }

    uint64_t maxTimeStamp = maxNanoSeconds * timerFreqquency / nsecPerSec;

    auto bits = std::bitset<64>(maxTimeStamp);
    validBits = bits.count();

    return retVal;
}

bool OaMetricSourceImp::isAvailable() {
    return isInitialized();
}

ze_result_t OaMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    DeviceImp *pDeviceImp = static_cast<DeviceImp *>(commandList.getDevice());

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
    return activationTracker->isMetricGroupActivated(hMetricGroup);
}

bool OaMetricSourceImp::isMetricGroupActivatedInHw() const {
    return activationTracker->isMetricGroupActivatedInHw();
}

bool OaMetricSourceImp::isImplicitScalingCapable() const {
    return metricDeviceContext.isImplicitScalingCapable();
}

void OaMetricSourceImp::setMetricOsInterface(std::unique_ptr<MetricOAOsInterface> &metricOAOsInterface) {
    this->metricOAOsInterface = std::move(metricOAOsInterface);
}

ze_result_t OaMetricSourceImp::activateMetricGroupsPreferDeferred(uint32_t count,
                                                                  zet_metric_group_handle_t *phMetricGroups) {
    activationTracker->activateMetricGroupsDeferred(count, phMetricGroups);
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricSourceImp::activateMetricGroupsAlreadyDeferred() {
    return activationTracker->activateMetricGroupsAlreadyDeferred();
}

ze_result_t OaMetricSourceImp::getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                                         uint32_t *pConcurrentGroupCount,
                                                         uint32_t *pCountPerConcurrentGroup) {

    if (*pConcurrentGroupCount == 0) {
        *pConcurrentGroupCount = static_cast<uint32_t>(hMetricGroups.size());
        return ZE_RESULT_SUCCESS;
    }

    *pConcurrentGroupCount = std::min(*pConcurrentGroupCount, static_cast<uint32_t>(hMetricGroups.size()));
    // Each metric group is in unique container
    for (uint32_t index = 0; index < *pConcurrentGroupCount; index++) {
        pCountPerConcurrentGroup[index] = 1;
    }

    return ZE_RESULT_SUCCESS;
}

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const {
    return static_cast<OaMetricSourceImp &>(*metricSources.at(MetricSource::metricSourceTypeOa));
}

} // namespace L0
