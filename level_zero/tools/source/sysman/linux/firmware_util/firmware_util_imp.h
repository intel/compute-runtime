/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util.h"

#include <cinttypes>
#include <mutex>
#include <string>
#include <vector>

namespace L0 {
typedef int (*pIgscDeviceInitByDevice)(struct igsc_device_handle *handle,
                                       const char *devicePath);
typedef int (*pIgscDeviceGetDeviceInfo)(struct igsc_device_handle *handle,
                                        struct igsc_info_device *info);
typedef int (*pIgscDeviceFwVersion)(struct igsc_device_handle *handle,
                                    struct igsc_fw_version *version);
typedef int (*pIgscDeviceIteratorCreate)(struct igsc_device_iterator **iter);
typedef int (*pIgscDeviceIteratorNext)(struct igsc_device_iterator *iter,
                                       struct igsc_device_info *info);
typedef void (*pIgscDeviceIteratorDestroy)(struct igsc_device_iterator *iter);
typedef int (*pIgscDeviceFwUpdate)(struct igsc_device_handle *handle,
                                   const uint8_t *buffer,
                                   const uint32_t bufferLen,
                                   igsc_progress_func_t progressFunc,
                                   void *ctx);
typedef int (*pIgscImageOpromInit)(struct igsc_oprom_image **img,
                                   const uint8_t *buffer,
                                   uint32_t bufferLen);
typedef int (*pIgscImageOpromType)(struct igsc_oprom_image *img,
                                   uint32_t *opromType);
typedef int (*pIgscDeviceOpromUpdate)(struct igsc_device_handle *handle,
                                      uint32_t opromType,
                                      struct igsc_oprom_image *img,
                                      igsc_progress_func_t progressFunc,
                                      void *ctx);
typedef int (*pIgscDeviceOpromVersion)(struct igsc_device_handle *handle,
                                       uint32_t opromType,
                                       struct igsc_oprom_version *version);

typedef int (*pIgscDeviceClose)(struct igsc_device_handle *handle);

extern pIgscDeviceInitByDevice deviceInitByDevice;
extern pIgscDeviceGetDeviceInfo deviceGetDeviceInfo;
extern pIgscDeviceFwVersion deviceGetFwVersion;
extern pIgscDeviceIteratorCreate deviceIteratorCreate;
extern pIgscDeviceIteratorNext deviceItreatorNext;
extern pIgscDeviceIteratorDestroy deviceItreatorDestroy;
extern pIgscDeviceFwUpdate deviceFwUpdate;
extern pIgscImageOpromInit imageOpromInit;
extern pIgscImageOpromType imageOpromType;
extern pIgscDeviceOpromUpdate deviceOpromUpdate;
extern pIgscDeviceOpromVersion deviceOpromVersion;
extern pIgscDeviceClose deviceClose;

class FirmwareUtilImp : public FirmwareUtil, NEO::NonCopyableOrMovableClass {
  public:
    FirmwareUtilImp(const std::string &pciBDF);
    ~FirmwareUtilImp();
    ze_result_t fwDeviceInit() override;
    ze_result_t getFirstDevice(igsc_device_info *) override;
    ze_result_t getFwVersion(std::string fwType, std::string &firmwareVersion) override;
    ze_result_t flashFirmware(std::string fwType, void *pImage, uint32_t size) override;
    ze_result_t fwIfrApplied(bool &ifrStatus) override;
    ze_result_t fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) override;
    ze_result_t fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) override;
    virtual ze_result_t fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) override;
    void getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) override;

    ze_result_t fwGetVersion(std::string &fwVersion);
    ze_result_t opromGetVersion(std::string &fwVersion);
    ze_result_t pscGetVersion(std::string &fwVersion);
    ze_result_t fwFlashGSC(void *pImage, uint32_t size);
    ze_result_t fwFlashOprom(void *pImage, uint32_t size);
    ze_result_t fwFlashIafPsc(void *pImage, uint32_t size);
    ze_result_t fwCallGetstatusExt(uint32_t &supportedTests, uint32_t &ifrApplied, uint32_t &prevErrors, uint32_t &pendingReset);

    std::string fwDevicePath{};
    struct igsc_device_handle fwDeviceHandle = {};
    bool loadEntryPoints();
    bool loadEntryPointsExt();

    NEO::OsLibrary *libraryHandle = nullptr;
    template <class T>
    bool getSymbolAddr(const std::string name, T &proc) {
        void *addr = libraryHandle->getProcAddress(name);
        proc = reinterpret_cast<T>(addr);
        return nullptr != proc;
    }

  private:
    uint16_t domain = 0;
    uint8_t bus = 0;
    uint8_t device = 0;
    uint8_t function = 0;
    std::mutex fwLock;
};
} // namespace L0
