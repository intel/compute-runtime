/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

ze_result_t PlatformMonitoringTech::readValue(const std::string &key, uint32_t &value) {

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

    if (res == ZE_RESULT_SUCCESS && bytesReturned != 0) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                          "Ioctl call could not return a valid value for register key %s\n", key.c_str());
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t PlatformMonitoringTech::readValue(const std::string &key, uint64_t &value) {
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

    if (res == ZE_RESULT_SUCCESS && bytesReturned != 0) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                          "Ioctl call could not return a valid value for register key %s\n", key.c_str());
    DEBUG_BREAK_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t PlatformMonitoringTech::getGuid() {
    // arbitrary upper-bound, tune if needed
    constexpr size_t sizeNeededMax = sizeof(PmtSysman::PmtTelemetryDiscovery) + (2 * PmtSysman::PmtMaxInterfaces - 1) * sizeof(PmtSysman::PmtTelemetryEntry);

    ze_result_t status;
    unsigned long sizeNeeded;
    PmtSysman::PmtTelemetryDiscovery *telemetryDiscovery = nullptr;

    // Get Telmetry Discovery size
    status = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemetryDiscoverySize, NULL, 0, (void *)&sizeNeeded, sizeof(sizeNeeded), NULL);
    if (status != ZE_RESULT_SUCCESS || sizeNeeded == 0 || sizeNeeded > sizeNeededMax) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Ioctl call could not return a valid value for the PMT interface telemetry size needed\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    telemetryDiscovery = (PmtSysman::PmtTelemetryDiscovery *)heapAllocFunction(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeNeeded);
    if (telemetryDiscovery == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Get Telmetry Discovery Structure
    status = ioctlReadWriteData(deviceInterface, PmtSysman::IoctlPmtGetTelemetryDiscovery, NULL, 0, (void *)telemetryDiscovery, sizeNeeded, NULL);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Ioctl call could not return a valid value for the PMT telemetry structure which provides the guids supported.\n");
        heapFreeFunction(GetProcessHeap(), 0, telemetryDiscovery);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto maxEntriesCount = (sizeNeeded - offsetof(PmtSysman::PmtTelemetryDiscovery, telemetry)) / sizeof(PmtSysman::PmtTelemetryEntry);
    if (telemetryDiscovery->count > maxEntriesCount) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Incorrect telemetry entries count.\n");
        heapFreeFunction(GetProcessHeap(), 0, telemetryDiscovery);
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    for (uint32_t i = 0; i < telemetryDiscovery->count; i++) {
        if (telemetryDiscovery->telemetry[i].index < PmtSysman::PmtMaxInterfaces) {
            guidToIndexList[telemetryDiscovery->telemetry[i].index] = telemetryDiscovery->telemetry[i].guid;
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Telemetry index is out of range.\n");
            heapFreeFunction(GetProcessHeap(), 0, telemetryDiscovery);
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return heapFreeFunction(GetProcessHeap(), 0, telemetryDiscovery) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(std::map<std::string, std::pair<uint32_t, uint32_t>> &keyOffsetMap) {
    int indexCount = 0;
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    if (pGuidToKeyOffsetMap == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    for (uint32_t index = 0; index < PmtSysman::PmtMaxInterfaces; index++) {
        std::map<std::string, uint32_t>::iterator it;
        auto keyOffsetMapEntry = pGuidToKeyOffsetMap->find(guidToIndexList[index]);
        if (keyOffsetMapEntry == pGuidToKeyOffsetMap->end()) {
            continue;
        } else {
            indexCount++;
            std::map<std::string, uint32_t> tempKeyOffsetMap = keyOffsetMapEntry->second;
            for (it = tempKeyOffsetMap.begin(); it != tempKeyOffsetMap.end(); it++) {
                keyOffsetMap.insert(std::make_pair(it->first, std::make_pair(it->second, index)));
            }
        }
    }
    if (indexCount == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
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

std::unique_ptr<PlatformMonitoringTech> PlatformMonitoringTech::create(SysmanProductHelper *pSysmanProductHelper, uint32_t bus, uint32_t device, uint32_t function) {
    std::unique_ptr<PlatformMonitoringTech> pPmt;
    pPmt = std::make_unique<PlatformMonitoringTech>(pSysmanProductHelper, bus, device, function);
    UNRECOVERABLE_IF(nullptr == pPmt);

    if ((pPmt->getDeviceInterface() != ZE_RESULT_SUCCESS) || (pPmt->init() != ZE_RESULT_SUCCESS)) {
        pPmt.reset(nullptr);
        return nullptr;
    }
    return pPmt;
}

PlatformMonitoringTech::~PlatformMonitoringTech() {
}

ze_result_t PlatformMonitoringTech::getDeviceRegistryProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA *pDeviceInfoData, DWORD devicePropertyType, std::vector<uint8_t> &dataBuffer) {

    DWORD dataSizeInBytes;

    if (NEO::SysCalls::setupDiGetDeviceRegistryProperty(deviceInfoSet, pDeviceInfoData, devicePropertyType, nullptr, nullptr, 0, &dataSizeInBytes)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    dataBuffer.resize(dataSizeInBytes);
    if (!NEO::SysCalls::setupDiGetDeviceRegistryProperty(deviceInfoSet, pDeviceInfoData, devicePropertyType, nullptr, dataBuffer.data(), (DWORD)dataBuffer.size(), nullptr)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

bool PlatformMonitoringTech::isInstancePciBdfMatchingWithWddmDevice(HDEVINFO hDevInfo, SP_DEVINFO_DATA *pDeviceInfoData) {
    std::vector<uint8_t> dataBuffer;
    uint32_t devicePciBus = 0;
    uint32_t devicePciDevice = 0;
    uint32_t devicePciFunction = 0;

    // Fetch device pci bus number
    ze_result_t result = getDeviceRegistryProperty(hDevInfo, pDeviceInfoData, SPDRP_BUSNUMBER, dataBuffer);
    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }

    memcpy_s(&devicePciBus, sizeof(uint32_t), dataBuffer.data(), sizeof(uint8_t) * dataBuffer.size());

    if (devicePciBus != this->pciBus) {
        return false;
    }

    dataBuffer.resize(0);
    uint32_t deviceAddress = 0;

    // Fetch device pci address. This provides the pci device and function numbers
    result = getDeviceRegistryProperty(hDevInfo, pDeviceInfoData, SPDRP_ADDRESS, dataBuffer);
    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }

    memcpy_s(&deviceAddress, sizeof(uint32_t), dataBuffer.data(), sizeof(uint8_t) * dataBuffer.size());
    devicePciDevice = (deviceAddress >> 16) & 0xffff;
    devicePciFunction = deviceAddress & 0xffff;

    if ((devicePciDevice != this->pciDevice) || (devicePciFunction != this->pciFunction)) {
        return false;
    }

    return true;
}

ze_result_t PlatformMonitoringTech::getDeviceInstanceId(DEVINST deviceInstance, std::wstring &deviceInstanceId) {
    unsigned long dataSizeInBytes = 0;

    CONFIGRET result = NEO::SysCalls::cmGetDeviceIdSize(&dataSizeInBytes, deviceInstance, 0);
    if (result != CR_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::vector<wchar_t> dataBuffer(dataSizeInBytes + 1, 0);
    result = NEO::SysCalls::cmGetDeviceId(deviceInstance, dataBuffer.data(), (ULONG)dataBuffer.size(), 0);
    if (result != CR_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    deviceInstanceId = std::wstring(dataBuffer.begin(), dataBuffer.end() - 1);
    return ZE_RESULT_SUCCESS;
}

void PlatformMonitoringTech::getAllChildDeviceInterfaces(HDEVINFO hDevInfo, DEVINST deviceInstance, uint32_t level, std::vector<std::wstring> &deviceInstanceIdList) {

    // According to Microsoft's guide we need to follow the below steps to find all the children for a device instance.
    // Call the CM_Get_Child function to retrieve a device instance handle to the first child device that is associated with a device instance.
    // Call CM_Get_Sibling as many times as it is necessary to enumerate all the sibling devices of the first child device that was retrieved by the call to CM_Get_Child.
    // Call CM_Get_Device_ID to retrieve the device instance identifiers that are associated with the device instance handles that were returned by the calls to CM_Get_Child and CM_Get_Sibling.

    std::wstring deviceInstanceId;
    if ((getDeviceInstanceId(deviceInstance, deviceInstanceId) != ZE_RESULT_SUCCESS) || deviceInstanceId.empty()) {
        return;
    }

    if (level > 1) {
        deviceInstanceIdList.push_back(deviceInstanceId);
    }

    // Get the SP_DEVINFO_DATA for the device
    SP_DEVINFO_DATA deviceInfoData = {};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!NEO::SysCalls::setupDiOpenDeviceInfo(hDevInfo, deviceInstanceId.c_str(), nullptr, 0, &deviceInfoData)) {
        return;
    }

    DEVINST childDeviceInstance = 0;
    if (NEO::SysCalls::cmGetChild(&childDeviceInstance, deviceInstance, 0) == CR_SUCCESS) {
        getAllChildDeviceInterfaces(hDevInfo, childDeviceInstance, level + 1, deviceInstanceIdList);

        DEVINST siblingDeviceInstance;
        while (NEO::SysCalls::cmGetSibling(&siblingDeviceInstance, childDeviceInstance, 0) == CR_SUCCESS) {
            getAllChildDeviceInterfaces(hDevInfo, siblingDeviceInstance, level + 1, deviceInstanceIdList);
            childDeviceInstance = siblingDeviceInstance;
        }
    }
}

ze_result_t PlatformMonitoringTech::getDeviceInterface() {
    std::wstring pmtDeviceInstanceId;
    GUID displayAdapter = PmtSysman::GuidIntefaceDisplayAdapter;
    GUID pmtTelemetry = PmtSysman::GuidIntefacePmtTelemetry;

    HDEVINFO hDevInfo = NEO::SysCalls::setupDiGetClassDevs(&displayAdapter, NULL, NULL, DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    SP_DEVINFO_DATA deviceInfoData = {};
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (DWORD i = 0; NEO::SysCalls::setupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); i++) {
        if (isInstancePciBdfMatchingWithWddmDevice(hDevInfo, &deviceInfoData)) {
            std::vector<std::wstring> deviceInstanceIdList;

            // Get all child devices
            getAllChildDeviceInterfaces(hDevInfo, deviceInfoData.DevInst, 1, deviceInstanceIdList);

            // Find the PMT device instance ID
            for (auto &deviceInstanceId : deviceInstanceIdList) {
                if (deviceInstanceId.find(L"INTC_PMT") != std::wstring::npos) {
                    pmtDeviceInstanceId = deviceInstanceId;
                    break;
                }
            }
            break;
        }
    }

    if (NEO::SysCalls::setupDiDestroyDeviceInfoList(hDevInfo) == FALSE) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    unsigned long cmListCharCount = 0;
    CONFIGRET status = CR_SUCCESS;
    std::vector<wchar_t> deviceInterfaceList;

    do {
        // Get the total size of list of all instances
        // N.B. Size returned is total length in "characters"
        status = NEO::SysCalls::cmGetDeviceInterfaceListSize(&cmListCharCount, (LPGUID)&pmtTelemetry, &pmtDeviceInstanceId[0], CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (status != CR_SUCCESS) {
            break;
        }
        // Free previous allocation if present.
        if (!deviceInterfaceList.empty()) {
            deviceInterfaceList.clear();
        }
        // Allocate buffer
        deviceInterfaceList.resize(cmListCharCount);

        if (deviceInterfaceList.empty()) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "Could not allocate memory to store the PMT device interface path.\n");
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        // N.B. cmListCharCount is length in characters
        status = NEO::SysCalls::cmGetDeviceInterfaceList((LPGUID)&pmtTelemetry, &pmtDeviceInstanceId[0], deviceInterfaceList.data(), cmListCharCount, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    } while (status == CR_BUFFER_SMALL);

    if (status != CR_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Could not find and store the PMT device interface path.\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    wchar_t *pDeviceInterface = deviceInterfaceList.data();

    while (*pDeviceInterface) {
        std::wstring deviceInterfaceName(pDeviceInterface);
        std::wstring::size_type found = deviceInterfaceName.find(L"INTC_PMT");
        size_t interfaceCharCount = wcslen(pDeviceInterface);
        if (found != std::wstring::npos) {
            deviceInterface = std::move(deviceInterfaceName);
            break;
        }
        pDeviceInterface += interfaceCharCount + 1;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t PlatformMonitoringTech::ioctlReadWriteData(std::wstring &path, uint32_t ioctl, void *bufferIn, uint32_t inSize, void *bufferOut, uint32_t outSize, uint32_t *sizeReturned) {
    void *handle;
    BOOL status = FALSE;

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

    this->pcloseHandle(handle);

    if (status == FALSE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "deviceIoControl call failed\n");
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
