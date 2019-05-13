/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/engine_info.h"
#include "runtime/os_interface/linux/memory_info.h"
#include "runtime/utilities/api_intercept.h"

#include "igfxfmid.h"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

struct GT_SYSTEM_INFO;

namespace NEO {
#define I915_CONTEXT_PRIVATE_PARAM_BOOST 0x80000000

class DeviceFactory;
struct HardwareInfo;

struct DeviceDescriptor {
    unsigned short deviceId;
    const HardwareInfo *pHwInfo;
    void (*setupHardwareInfo)(HardwareInfo *, bool);
    GTTYPE eGtType;
};

extern const DeviceDescriptor deviceDescriptorTable[];

class Drm {
    friend DeviceFactory;

  public:
    static Drm *get(int32_t deviceOrdinal);

    virtual ~Drm();

    virtual int ioctl(unsigned long request, void *arg);

    int getDeviceID(int &devId);
    int getDeviceRevID(int &revId);
    int getExecSoftPin(int &execSoftPin);
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);

    bool is48BitAddressRangeSupported();
    bool isPreemptionSupported() const { return preemptionSupported; }
    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    int getFileDescriptor() const { return fd; }
    uint32_t createDrmContext();
    void destroyDrmContext(uint32_t drmContextId);
    void setLowPriorityContextParam(uint32_t drmContextId);
    int bindDrmContext(uint32_t drmContextId, DeviceBitfield deviceBitfield, aub_stream::EngineType engineType);

    void setGtType(GTTYPE eGtType) { this->eGtType = eGtType; }
    GTTYPE getGtType() const { return this->eGtType; }
    MOCKABLE_VIRTUAL int getErrno();
    void setSimplifiedMocsTableUsage(bool value);
    bool getSimplifiedMocsTableUsage() const;
    void queryEngineInfo();
    void queryMemoryInfo();

    MemoryInfo *getMemoryInfo() const {
        return memoryInfo.get();
    }

  protected:
    bool useSimplifiedMocsTable = false;
    bool preemptionSupported = false;
    int fd;
    int deviceId = 0;
    int revisionId = 0;
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    Drm(int fd) : fd(fd) {}
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;

    static bool isi915Version(int fd);
    static int getDeviceFd(const int devType);
    static int openDevice();
    static Drm *create(int32_t deviceOrdinal);
    static void closeDevice(int32_t deviceOrdinal);

    std::string getSysFsPciPath(int deviceID);
    void *query(uint32_t queryId);

#pragma pack(1)
    struct PCIConfig {
        uint16_t VendorID;
        uint16_t DeviceID;
        uint16_t Command;
        uint16_t Status;
        uint8_t Revision;
        uint8_t ProgIF;
        uint8_t Subclass;
        uint8_t ClassCode;
        uint8_t cacheLineSize;
        uint8_t LatencyTimer;
        uint8_t HeaderType;
        uint8_t BIST;
        uint32_t BAR0[6];
        uint32_t CardbusCISPointer;
        uint16_t SubsystemVendorID;
        uint16_t SubsystemDeviceID;
        uint32_t ROM;
        uint8_t Capabilities;
        uint8_t Reserved[7];
        uint8_t InterruptLine;
        uint8_t InterruptPIN;
        uint8_t MinGrant;
        uint8_t MaxLatency;
    };
#pragma pack()
    static const char *sysFsDefaultGpuPath;
    static const char *maxGpuFrequencyFile;
    static const char *configFileName;

  private:
    int getParamIoctl(int param, int *dstValue);
};
} // namespace NEO
