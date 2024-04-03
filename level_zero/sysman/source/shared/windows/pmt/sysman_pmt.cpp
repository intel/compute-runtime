/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

namespace L0 {
namespace Sysman {

ze_result_t PlatformMonitoringTech::readValue(const std::string key, uint32_t &value) {

    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Key %s has not been defined in key offset map.\n", key.c_str());
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    PmtSysman::PmtTelemetryRead readRequest = {0};
    readRequest.version = 0;
    readRequest.index = (offset->second).second;
    readRequest.offset = baseOffset + (offset->second).first;
    readRequest.count = 1;

    uint32_t bytesReturned = 0;

    auto res = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemtryDword, (void *)&readRequest, sizeof(PmtSysman::PmtTelemetryRead), &value, sizeof(uint32_t), &bytesReturned);

    if (res == ZE_RESULT_SUCCESS && value != NULL && bytesReturned != 0) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                          "Ioctl call could not return a valid value for register key %s\n", key.c_str());
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t PlatformMonitoringTech::readValue(const std::string key, uint64_t &value) {
    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Key %s has not been defined in key offset map.\n", key.c_str());
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    PmtSysman::PmtTelemetryRead readRequest = {0};
    readRequest.version = 0;
    readRequest.index = (offset->second).second;
    readRequest.offset = baseOffset + (offset->second).first;
    readRequest.count = 1;

    uint32_t bytesReturned = 0;

    auto res = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemtryQword, (void *)&readRequest, sizeof(PmtSysman::PmtTelemetryRead), &value, sizeof(uint64_t), &bytesReturned);

    if (res == ZE_RESULT_SUCCESS && value != NULL && bytesReturned != 0) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                          "Ioctl call could not return a valid value for register key %s\n", key.c_str());
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t PlatformMonitoringTech::getGuid() {
    int status;
    unsigned long sizeNeeded;
    PmtSysman::PmtTelemetryDiscovery *telemetryDiscovery;

    // Get Telmetry Discovery size
    status = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemetryDiscoverySize, NULL, 0, (void *)&sizeNeeded, sizeof(sizeNeeded), NULL);

    if (status != ZE_RESULT_SUCCESS || sizeNeeded == 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Ioctl call could not return a valid value for the PMT interface telemetry size needed\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    telemetryDiscovery = (PmtSysman::PmtTelemetryDiscovery *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeNeeded);

    // Get Telmetry Discovery Structure
    status = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemetryDiscovery, NULL, 0, (void *)telemetryDiscovery, sizeNeeded, NULL);

    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Ioctl call could not return a valid value for the PMT telemetry structure which provides the guids supported.\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    for (uint32_t x = 0; x < telemetryDiscovery->count; x++) {
        guidToIndexList[telemetryDiscovery->telemetry[x].index] = telemetryDiscovery->telemetry[x].guid;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t PlatformMonitoringTech::init() {
    ze_result_t result = getGuid();
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    result = getKeyOffsetMap(keyOffsetMap);
    return result;
}

std::unique_ptr<PlatformMonitoringTech> PlatformMonitoringTech::create() {
    std::vector<wchar_t> deviceInterface;
    if (enumeratePMTInterface(&PmtSysman::GuidIntefacePmtTelemetry, deviceInterface) == ZE_RESULT_SUCCESS) {
        std::unique_ptr<PlatformMonitoringTech> pPmt;
        pPmt = std::make_unique<PlatformMonitoringTech>(deviceInterface);
        UNRECOVERABLE_IF(nullptr == pPmt);
        if (pPmt->init() != ZE_RESULT_SUCCESS) {
            pPmt.reset(nullptr);
        }
        return pPmt;
    }
    return nullptr;
}

PlatformMonitoringTech::~PlatformMonitoringTech() {
}

ze_result_t PlatformMonitoringTech::enumeratePMTInterface(const GUID *guid, std::vector<wchar_t> &deviceInterface) {

    unsigned long cmListCharCount = 0;
    CONFIGRET status = CR_SUCCESS;

    do {
        // Get the total size of list of all instances
        // N.B. Size returned is total length in "characters"
        status = NEO::SysCalls::cmGetDeviceInterfaceListSize(&cmListCharCount, (LPGUID)guid, NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (status != CR_SUCCESS) {
            break;
        }
        // Free previous allocation if present.
        if (!deviceInterface.empty()) {
            deviceInterface.clear();
        }
        // Allocate buffer
        deviceInterface.resize(cmListCharCount);

        if (deviceInterface.empty()) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Could not allocate memory to store the PMT device interface path.\n");
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        // N.B. cmListCharCount is length in characters
        status = NEO::SysCalls::cmGetDeviceInterfaceList((LPGUID)guid, NULL, &deviceInterface[0], cmListCharCount, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

    } while (status == CR_BUFFER_SMALL);

    if (status != CR_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Could not find and store the PMT device inteface path.\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t PlatformMonitoringTech::ioctlReadWriteData(std::vector<wchar_t> path, uint32_t ioctl, void *bufferIn, uint32_t inSize, void *bufferOut, uint32_t outSize, uint32_t *sizeReturned) {

    void *handle;
    int32_t status = FALSE;

    if (path.empty()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "PMT interface path is empty.\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Open handle to driver
    handle = this->pcreateFile(&path[0], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Could not open the pmt interface path %s.\n", &path[0]);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    // Call DeviceIoControl
    status = this->pdeviceIoControl(handle, ioctl, bufferIn, inSize, bufferOut, outSize, reinterpret_cast<unsigned long *>(sizeReturned), NULL);

    if (status == FALSE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "deviceIoControl call failed\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    this->pcloseHandle(handle);

    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
