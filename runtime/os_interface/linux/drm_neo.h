/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "igfxfmid.h"
#include "runtime/utilities/api_intercept.h"

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <string>

struct GT_SYSTEM_INFO;

namespace OCLRT {
#define I915_CONTEXT_PRIVATE_PARAM_BOOST 0x80000000

class DeviceFactory;
struct HardwareInfo;

struct DeviceDescriptor {
    unsigned short deviceId;
    const HardwareInfo *pHwInfo;
    void (*setupGtSystemInfo)(GT_SYSTEM_INFO *);
    GTTYPE eGtType;
};

extern const DeviceDescriptor deviceDescriptorTable[];

class Drm {
    friend DeviceFactory;

  public:
    uint32_t lowPriorityContextId;
    static Drm *get(int32_t deviceOrdinal);

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
    MOCKABLE_VIRTUAL bool hasPreemption();
    bool setLowPriority();
    int getFileDescriptor() const { return fd; }
    bool contextCreate();
    void contextDestroy();

    void setGtType(GTTYPE eGtType) { this->eGtType = eGtType; }
    GTTYPE getGtType() const { return this->eGtType; }
    MOCKABLE_VIRTUAL int getErrno();
    void setSimplifiedMocsTableUsage(bool value);
    bool getSimplifiedMocsTableUsage() const;

  protected:
    bool useSimplifiedMocsTable = false;
    int fd;
    int deviceId;
    int revisionId;
    GTTYPE eGtType;
    Drm(int fd) : lowPriorityContextId(0), fd(fd), deviceId(0), revisionId(0), eGtType(GTTYPE_UNDEFINED) {}
    virtual ~Drm();

    static bool isi915Version(int fd);
    static int getDeviceFd(const int devType);
    static int openDevice();
    static Drm *create(int32_t deviceOrdinal);
    static void closeDevice(int32_t deviceOrdinal);

    std::string getSysFsPciPath(int deviceID);

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
} // namespace OCLRT
