/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/pdh_interface_win.h"

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <algorithm>
#include <format>

namespace NEO {

PdhInterfaceWindows::PdhInterfaceWindows(ExecutionEnvironment &executionEnvironment)
    : executionEnvironment(executionEnvironment) {
}

PdhInterfaceWindows::~PdhInterfaceWindows() {
    if (pdhInitialized && memoryUsageQuery) {
        pdhCloseQueryFunction(memoryUsageQuery);
    }
}

uint64_t PdhInterfaceWindows::getCurrentMemoryUsage(uint32_t rootDeviceIndex, bool dedicatedCounter) {
    if (!initPdh()) {
        return 0;
    }

    auto &wddm = getWddm(rootDeviceIndex);
    std::wstring counterPath;
    if (dedicatedCounter) {
        counterPath = constructPdhCounterString(L"GPU Adapter Memory", L"Dedicated Usage", wddm.getAdapterLuid(), 0);
    } else {
        counterPath = constructPdhCounterString(L"GPU Adapter Memory", L"Shared Usage", wddm.getAdapterLuid(), 0);
    }

    if (counterHandles.find(counterPath) == counterHandles.end()) {
        PDH_HCOUNTER counterHandle = nullptr;
        if (pdhAddEnglishCounterWFunction(memoryUsageQuery, counterPath.c_str(), 0, &counterHandle) != ERROR_SUCCESS) {
            return 0;
        }
        counterHandles[counterPath] = counterHandle;
    }

    if (pdhCollectQueryDataFunction(memoryUsageQuery) != ERROR_SUCCESS) {
        return 0;
    }

    PDH_FMT_COUNTERVALUE counterValue{};
    if (pdhGetFormattedCounterValueFunction(counterHandles[counterPath], PDH_FMT_LARGE, nullptr, &counterValue) != ERROR_SUCCESS) {
        return 0;
    }

    return static_cast<uint64_t>(std::max<LONGLONG>(0, counterValue.largeValue));
}

bool PdhInterfaceWindows::initPdh() {
    std::call_once(pdhInitOnceFlag, [this]() {
        pdhLibrary.reset(OsLibrary::loadFunc({"pdh.dll"}));

        if (!pdhLibrary || !pdhLibrary->isLoaded()) {
            return;
        }

        pdhOpenQueryFunction = reinterpret_cast<PdhOpenQueryWFn>(pdhLibrary->getProcAddress("PdhOpenQueryW"));
        pdhAddEnglishCounterWFunction = reinterpret_cast<PdhAddEnglishCounterWFn>(pdhLibrary->getProcAddress("PdhAddEnglishCounterW"));
        pdhCollectQueryDataFunction = reinterpret_cast<PdhCollectQueryDataFn>(pdhLibrary->getProcAddress("PdhCollectQueryData"));
        pdhGetFormattedCounterValueFunction = reinterpret_cast<PdhGetFormattedCounterValueFn>(pdhLibrary->getProcAddress("PdhGetFormattedCounterValue"));
        pdhCloseQueryFunction = reinterpret_cast<PdhCloseQueryFn>(pdhLibrary->getProcAddress("PdhCloseQuery"));

        if (!pdhOpenQueryFunction || !pdhAddEnglishCounterWFunction || !pdhCollectQueryDataFunction || !pdhGetFormattedCounterValueFunction || !pdhCloseQueryFunction) {
            return;
        }

        if (pdhOpenQueryFunction(nullptr, 0, &memoryUsageQuery) != ERROR_SUCCESS) {
            return;
        }

        pdhInitialized = true;
    });

    return pdhInitialized;
}

std::wstring PdhInterfaceWindows::constructPdhCounterString(const std::wstring &objectName, const std::wstring &counterName, LUID luid, uint32_t index) const {
    return std::format(L"\\{}(luid_{:#010X}_{:#010X}_phys_{})\\{}", objectName, static_cast<long>(luid.HighPart), static_cast<unsigned long>(luid.LowPart), index, counterName);
}

Wddm &PdhInterfaceWindows::getWddm(uint32_t rootDeviceIndex) const {
    return *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>();
}

std::unique_ptr<PdhInterface> PdhInterface::create(ExecutionEnvironment &executionEnvironment) {
    return std::make_unique<PdhInterfaceWindows>(executionEnvironment);
}

} // namespace NEO
