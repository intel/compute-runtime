/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/directory.h"

#include <map>

namespace L0 {
namespace Sysman {
static const std::map<std::string, csc_late_binding_type> lateBindingTypeToEnumMap = {
    {"FanTable", CSC_LATE_BINDING_TYPE_FAN_TABLE},
    {"VRConfig", CSC_LATE_BINDING_TYPE_VR_CONFIG},
};

const std::string fwDeviceInitByDevice = "igsc_device_init_by_device_info";
const std::string fwDeviceGetDeviceInfo = "igsc_device_get_device_info";
const std::string fwDeviceFwVersion = "igsc_device_fw_version";
const std::string fwDeviceIteratorCreate = "igsc_device_iterator_create";
const std::string fwDeviceIteratorNext = "igsc_device_iterator_next";
const std::string fwDeviceIteratorDestroy = "igsc_device_iterator_destroy";
const std::string fwDeviceFwUpdate = "igsc_device_fw_update";
const std::string fwImageOpromInit = "igsc_image_oprom_init";
const std::string fwImageOpromType = "igsc_image_oprom_type";
const std::string fwDeviceOpromUpdate = "igsc_device_oprom_update";
const std::string fwDeviceOpromVersion = "igsc_device_oprom_version";
const std::string fwDevicePscVersion = "igsc_device_psc_version";
const std::string fwDeviceClose = "igsc_device_close";
const std::string fwDeviceUpdateLateBindingConfig = "igsc_device_update_late_binding_config";

pIgscDeviceInitByDevice deviceInitByDevice;
pIgscDeviceGetDeviceInfo deviceGetDeviceInfo;
pIgscDeviceFwVersion deviceGetFwVersion;
pIgscDeviceIteratorCreate deviceIteratorCreate;
pIgscDeviceIteratorNext deviceItreatorNext;
pIgscDeviceIteratorDestroy deviceItreatorDestroy;
pIgscDeviceFwUpdate deviceFwUpdate;
pIgscImageOpromInit imageOpromInit;
pIgscImageOpromType imageOpromType;
pIgscDeviceOpromUpdate deviceOpromUpdate;
pIgscDeviceOpromVersion deviceOpromVersion;
pIgscDevicePscVersion deviceGetPscVersion;
pIgscDeviceClose deviceClose;
pIgscDeviceUpdateLateBindingConfig deviceUpdateLateBindingConfig;

bool FirmwareUtilImp::loadEntryPoints() {
    bool ok = getSymbolAddr(fwDeviceInitByDevice, deviceInitByDevice);
    ok = ok && getSymbolAddr(fwDeviceGetDeviceInfo, deviceGetDeviceInfo);
    ok = ok && getSymbolAddr(fwDeviceFwVersion, deviceGetFwVersion);
    ok = ok && getSymbolAddr(fwDeviceIteratorCreate, deviceIteratorCreate);
    ok = ok && getSymbolAddr(fwDeviceIteratorNext, deviceItreatorNext);
    ok = ok && getSymbolAddr(fwDeviceIteratorDestroy, deviceItreatorDestroy);
    ok = ok && getSymbolAddr(fwDeviceFwUpdate, deviceFwUpdate);
    ok = ok && getSymbolAddr(fwImageOpromInit, imageOpromInit);
    ok = ok && getSymbolAddr(fwImageOpromType, imageOpromType);
    ok = ok && getSymbolAddr(fwDeviceOpromUpdate, deviceOpromUpdate);
    ok = ok && getSymbolAddr(fwDeviceOpromVersion, deviceOpromVersion);
    ok = ok && getSymbolAddr(fwDevicePscVersion, deviceGetPscVersion);
    ok = ok && getSymbolAddr(fwDeviceClose, deviceClose);
    ok = ok && getSymbolAddr(fwDeviceUpdateLateBindingConfig, deviceUpdateLateBindingConfig);
    ok = ok && loadEntryPointsExt();

    return ok;
}

void firmwareFlashProgressFunc(uint32_t done, uint32_t total, void *ctx) {
    if (ctx != nullptr) {
        uint32_t percent = (done * 100) / total;
        FirmwareUtilImp *pFirmwareUtilImp = static_cast<FirmwareUtilImp *>(ctx);
        pFirmwareUtilImp->updateFirmwareFlashProgress(percent);
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stdout, "Progess: %d/%d:%d/%\n", done, total, percent);
    }
}

void FirmwareUtilImp::updateFirmwareFlashProgress(uint32_t percent) {
    const std::lock_guard<std::mutex> lock(flashProgress.fwProgressLock);
    flashProgress.completionPercent = percent;
}

ze_result_t FirmwareUtilImp::getFlashFirmwareProgress(uint32_t *pCompletionPercent) {
    const std::lock_guard<std::mutex> lock(flashProgress.fwProgressLock);
    *pCompletionPercent = flashProgress.completionPercent;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::getFirstDevice(IgscDeviceInfo *info) {
    igsc_device_iterator *iter;
    int ret = deviceIteratorCreate(&iter);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    info->name[0] = '\0';
    do {
        ret = deviceItreatorNext(iter, info);
        if (ret != IGSC_SUCCESS) {
            deviceItreatorDestroy(iter);
            return ZE_RESULT_ERROR_UNINITIALIZED;
        }
        if (info->domain == domain &&
            info->bus == bus &&
            info->dev == device &&
            info->func == function) {
            fwDevicePath.assign(info->name);
            break;
        }
    } while (1);

    deviceItreatorDestroy(iter);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwDeviceInit() {
    int ret;
    IgscDeviceInfo info;
    ze_result_t result = getFirstDevice(&info);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    ret = deviceInitByDevice(&fwDeviceHandle, fwDevicePath.c_str());
    if (ret != 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwGetVersion(std::string &fwVersion) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    igsc_fw_version deviceFwVersion;
    memset(&deviceFwVersion, 0, sizeof(deviceFwVersion));
    int ret = deviceGetFwVersion(&fwDeviceHandle, &deviceFwVersion);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    fwVersion.append(deviceFwVersion.project);
    fwVersion.append("_");
    fwVersion.append(std::to_string(deviceFwVersion.hotfix));
    fwVersion.append(".");
    fwVersion.append(std::to_string(deviceFwVersion.build));
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::opromGetVersion(std::string &fwVersion) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    igsc_oprom_version opromVersion;
    memset(&opromVersion, 0, sizeof(opromVersion));
    int ret = deviceOpromVersion(&fwDeviceHandle, IGSC_OPROM_CODE, &opromVersion);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    fwVersion.append("OPROM CODE VERSION:");
    for (int i = 0; i < IGSC_OPROM_VER_SIZE; i++) {
        fwVersion.append(std::to_string(static_cast<unsigned int>(opromVersion.version[i])));
    }
    fwVersion.append("_");
    memset(&opromVersion, 0, sizeof(opromVersion));
    ret = deviceOpromVersion(&fwDeviceHandle, IGSC_OPROM_DATA, &opromVersion);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    fwVersion.append("OPROM DATA VERSION:");
    for (int i = 0; i < IGSC_OPROM_VER_SIZE; i++) {
        fwVersion.append(std::to_string(static_cast<unsigned int>(opromVersion.version[i])));
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwFlashGSC(void *pImage, uint32_t size) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    int ret = deviceFwUpdate(&fwDeviceHandle, static_cast<const uint8_t *>(pImage), size, firmwareFlashProgressFunc, this);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwFlashOprom(void *pImage, uint32_t size) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    struct igsc_oprom_image *opromImg = nullptr;
    uint32_t opromImgType = 0;
    int retData = 0, retCode = 0;
    int ret = imageOpromInit(&opromImg, static_cast<const uint8_t *>(pImage), size);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    ret = imageOpromType(opromImg, &opromImgType);
    if (ret != IGSC_SUCCESS) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    if (opromImgType & IGSC_OPROM_DATA) {
        retData = deviceOpromUpdate(&fwDeviceHandle, IGSC_OPROM_DATA, opromImg, firmwareFlashProgressFunc, this);
    }
    if (opromImgType & IGSC_OPROM_CODE) {
        retCode = deviceOpromUpdate(&fwDeviceHandle, IGSC_OPROM_CODE, opromImg, firmwareFlashProgressFunc, this);
    }
    if ((retData != IGSC_SUCCESS) && (retCode != IGSC_SUCCESS)) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareUtilImp::fwFlashLateBinding(void *pImage, uint32_t size, std::string fwType) {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    uint32_t lateBindingFlashStatus = 0;
    int ret = deviceUpdateLateBindingConfig(&fwDeviceHandle, lateBindingTypeToEnumMap.at(fwType), CSC_LATE_BINDING_FLAGS_IS_PERSISTENT_MASK, static_cast<uint8_t *>(pImage), static_cast<size_t>(size), &lateBindingFlashStatus);
    if (ret != IGSC_SUCCESS || lateBindingFlashStatus != CSC_LATE_BINDING_STATUS_SUCCESS) {
        if (ret == IGSC_ERROR_BUSY) {
            return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
        }
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

FirmwareUtilImp::FirmwareUtilImp(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function) : domain(domain), bus(bus), device(device), function(function) {
}

FirmwareUtilImp::~FirmwareUtilImp() {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    if (nullptr != libraryHandle) {
        deviceClose(&fwDeviceHandle);
        delete libraryHandle;
        libraryHandle = nullptr;
    }
};

FirmwareUtil *FirmwareUtil::create(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function) {
    FirmwareUtilImp *pFwUtilImp = new FirmwareUtilImp(domain, bus, device, function);
    UNRECOVERABLE_IF(nullptr == pFwUtilImp);
    NEO::OsLibraryCreateProperties properties(FirmwareUtilImp::fwUtilLibraryName);
    properties.customLoadFlags = &FirmwareUtilImp::fwUtilLoadFlags;
    pFwUtilImp->libraryHandle = NEO::OsLibrary::loadFunc(properties);
    if (pFwUtilImp->libraryHandle == nullptr || pFwUtilImp->loadEntryPoints() == false || pFwUtilImp->fwDeviceInit() != ZE_RESULT_SUCCESS) {
        if (nullptr != pFwUtilImp->libraryHandle) {
            delete pFwUtilImp->libraryHandle;
            pFwUtilImp->libraryHandle = nullptr;
        }
        delete pFwUtilImp;
        return nullptr;
    }
    return static_cast<FirmwareUtil *>(pFwUtilImp);
}
} // namespace Sysman
} // namespace L0
