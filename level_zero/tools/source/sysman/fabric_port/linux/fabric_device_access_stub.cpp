/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "fabric_device_access_stub.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {

ze_result_t FabricDeviceAccessStub::getState(const zes_fabric_port_id_t portId, zes_fabric_port_state_t &state) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::getThroughput(const zes_fabric_port_id_t portId, zes_fabric_port_throughput_t &throughput) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::getPortEnabledState(const zes_fabric_port_id_t portId, bool &enabled) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::getPortBeaconState(const zes_fabric_port_id_t portId, bool &enabled) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::enablePortBeaconing(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::disablePortBeaconing(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::enable(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::disable(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::enableUsage(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::disableUsage(const zes_fabric_port_id_t portId) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::forceSweep() {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::routingQuery(uint32_t &start, uint32_t &end) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::getPorts(std::vector<zes_fabric_port_id_t> &ports) {
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricDeviceAccessStub::getMultiPortThroughput(std::vector<zes_fabric_port_id_t> &portIdList, zes_fabric_port_throughput_t **pThroughput) {
    return ZE_RESULT_SUCCESS;
}

void FabricDeviceAccessStub::getProperties(const zes_fabric_port_id_t portId, std::string &model, bool &onSubdevice,
                                           uint32_t &subdeviceId, zes_fabric_port_speed_t &maxRxSpeed, zes_fabric_port_speed_t &maxTxSpeed) {
    model = "";
    onSubdevice = false;
    subdeviceId = 0U;
    maxRxSpeed.width = -1;
    maxRxSpeed.bitRate = -1L;
    maxTxSpeed.width = -1;
    maxTxSpeed.bitRate = -1L;
}

FabricDeviceAccessStub::FabricDeviceAccessStub(OsSysman *pOsSysman) {
}

FabricDeviceAccessStub::~FabricDeviceAccessStub() {
}

FabricDeviceAccess *FabricDeviceAccess::create(OsSysman *pOsSysman) {
    return new FabricDeviceAccessStub(pOsSysman);
}

} // namespace L0
