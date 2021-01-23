/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util.h"

#include <string>
#include <vector>

namespace L0 {
typedef int (*pIgscDeviceInitByDevice)(struct igsc_device_handle *handle,
                                       const char *device_path);
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
                                   const uint32_t buffer_len,
                                   igsc_progress_func_t progress_f,
                                   void *ctx);
typedef int (*pIgscImageOpromInit)(struct igsc_oprom_image **img,
                                   const uint8_t *buffer,
                                   uint32_t buffer_len);
typedef int (*pIgscImageOpromType)(struct igsc_oprom_image *img,
                                   uint32_t *oprom_type);
typedef int (*pIgscDeviceOpromUpdate)(struct igsc_device_handle *handle,
                                      uint32_t oprom_type,
                                      struct igsc_oprom_image *img,
                                      igsc_progress_func_t progress_f,
                                      void *ctx);
typedef int (*pIgscDeviceOpromVersion)(struct igsc_device_handle *handle,
                                       uint32_t oprom_type,
                                       struct igsc_oprom_version *version);

class FirmwareUtilImp : public FirmwareUtil, NEO::NonCopyableOrMovableClass {
  public:
    FirmwareUtilImp();
    ~FirmwareUtilImp();
    ze_result_t fwDeviceInit() override;
    ze_result_t getFirstDevice(igsc_device_info *) override;
    ze_result_t fwGetVersion(std::string &fwVersion) override;
    ze_result_t opromGetVersion(std::string &fwVersion) override;
    ze_result_t fwFlashGSC(void *pImage, uint32_t size) override;
    ze_result_t fwFlashOprom(void *pImage, uint32_t size) override;

    template <class T>
    bool getSymbolAddr(const std::string name, T &proc);

    std::string fwDevicePath;
    struct igsc_device_handle fwDeviceHandle;
    bool loadEntryPoints();

    NEO::OsLibrary *libraryHandle = nullptr;
    static const std::string fwUtilLibraryFile;
    static const std::string fwDeviceInitByDevice;
    static const std::string fwDeviceGetDeviceInfo;
    static const std::string fwDeviceFwVersion;
    static const std::string fwDeviceIteratorCreate;
    static const std::string fwDeviceIteratorNext;
    static const std::string fwDeviceIteratorDestroy;
    static const std::string fwDeviceFwUpdate;
    static const std::string fwImageOpromInit;
    static const std::string fwImageOpromType;
    static const std::string fwDeviceOpromUpdate;
    static const std::string fwDeviceOpromVersion;

    pIgscDeviceInitByDevice deviceInitByDevice = nullptr;
    pIgscDeviceGetDeviceInfo deviceGetDeviceInfo = nullptr;
    pIgscDeviceFwVersion deviceGetFwVersion = nullptr;
    pIgscDeviceIteratorCreate deviceIteratorCreate = nullptr;
    pIgscDeviceIteratorNext deviceItreatorNext = nullptr;
    pIgscDeviceIteratorDestroy deviceItreatorDestroy = nullptr;
    pIgscDeviceFwUpdate deviceFwUpdate = nullptr;
    pIgscImageOpromInit imageOpromInit = nullptr;
    pIgscImageOpromType imageOpromType = nullptr;
    pIgscDeviceOpromUpdate deviceOpromUpdate = nullptr;
    pIgscDeviceOpromVersion deviceOpromVersion = nullptr;
};
} // namespace L0