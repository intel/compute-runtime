/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

#include <algorithm>
#include <cstring>
#include <errno.h>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

namespace L0 {
namespace Sysman {
namespace ult {

inline constexpr size_t mockPciConfigHeaderSize = 64u;
inline constexpr uint32_t mockPciConfigVendorAndDeviceIdOffset = 0x00u;
inline constexpr uint32_t mockPciConfigRevisionIdOffset = 0x08u;
inline constexpr uint32_t mockPciConfigHeaderTypeOffset = 0x0eu;
inline constexpr uint32_t mockPciConfigBar0Offset = 0x10u;
inline const std::string mockPciSysfsDevicesDirectory = "/sys/bus/pci/devices";

inline constexpr uint16_t mockPciIntelVendorId = 0x8086u;
inline constexpr uint16_t mockPciNonIntelVendorId = 0x1002u;
inline uint16_t getMockPciDeviceId() {
    return NEO::deviceDescriptorTable[0].deviceId;
}
inline constexpr uint16_t mockPciNpuDeviceId = 0xad1du;
inline constexpr uint16_t mockPciUnknownDeviceId = 0xffffu;
inline constexpr uint16_t mockPciSubsystemVendorId = 0x8086u;
inline constexpr uint16_t mockPciSubsystemDeviceId = 0x1234u;
inline constexpr uint8_t mockPciRevision = 0x01u;
inline constexpr uint32_t mockPciDisplayControllerClassCode = 0x030000u;
inline constexpr uint32_t mockPciProcessingAcceleratorClassCode = 0x120000u;
inline constexpr uint32_t mockPciNetworkControllerClassCode = 0x020000u;

inline std::string getMockSysfsHexValue(uint32_t value, int digits) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setfill('0') << std::setw(digits) << value;
    return stream.str();
}

inline void setMockConfigDword(std::vector<uint8_t> &configHeader, uint32_t offset, uint32_t value) {
    for (uint32_t byteIndex = 0; byteIndex < sizeof(uint32_t); byteIndex++) {
        configHeader[offset + byteIndex] = static_cast<uint8_t>(value >> (byteIndex * 8));
    }
}

inline std::vector<uint8_t> getMockHealthyConfigHeader() {
    std::vector<uint8_t> configHeader(mockPciConfigHeaderSize, 0);
    setMockConfigDword(configHeader, mockPciConfigVendorAndDeviceIdOffset,
                       (static_cast<uint32_t>(getMockPciDeviceId()) << 16) | mockPciIntelVendorId);
    configHeader[mockPciConfigRevisionIdOffset] = mockPciRevision;
    configHeader[mockPciConfigHeaderTypeOffset] = 0x00u; // type 0, single function endpoint
    // BAR0 and BAR1, the two halves of a 64 bit memory BAR of a healthy device
    setMockConfigDword(configHeader, mockPciConfigBar0Offset, 0x53000004u);
    setMockConfigDword(configHeader, mockPciConfigBar0Offset + sizeof(uint32_t), 0x00000000u);
    return configHeader;
}

inline std::vector<uint8_t> getMockAllOnesConfigHeader() {
    return std::vector<uint8_t>(mockPciConfigHeaderSize, 0xffu);
}

struct MockPciSysfsDevice {
    std::map<std::string, std::string> attributes = {
        {"vendor", getMockSysfsHexValue(mockPciIntelVendorId, 4)},
        {"device", getMockSysfsHexValue(getMockPciDeviceId(), 4)},
        {"class", getMockSysfsHexValue(mockPciDisplayControllerClassCode, 6)},
        {"subsystem_vendor", getMockSysfsHexValue(mockPciSubsystemVendorId, 4)},
        {"subsystem_device", getMockSysfsHexValue(mockPciSubsystemDeviceId, 4)},
        {"revision", getMockSysfsHexValue(mockPciRevision, 2)},
    };
    bool isVirtualFunction = false;
    int attributeReadErrorNum = 0; // when non zero, reading any attribute node fails with this errno
    std::vector<uint8_t> configHeader = getMockHealthyConfigHeader();
    int configOpenErrorNum = 0; // when non zero, opening the config node fails with this errno
    int configReadErrorNum = 0; // errno reported alongside a short read
    ssize_t configBytesToRead = static_cast<ssize_t>(mockPciConfigHeaderSize);
};

class MockPciSysfs {
  public:
    MockPciSysfs() {
        pMockPciSysfs = this;
    }
    ~MockPciSysfs() {
        pMockPciSysfs = nullptr;
    }

    MockPciSysfsDevice &addDevice(const std::string &directoryName) {
        deviceOrder.push_back(directoryName);
        return devices[directoryName];
    }

    void setupDirectoryListing() {
        std::vector<std::string> devicePaths;
        for (const auto &directoryName : deviceOrder) {
            devicePaths.push_back(mockPciSysfsDevicesDirectory + "/" + directoryName);
        }
        NEO::directoryFilesMap[mockPciSysfsDevicesDirectory] = devicePaths;
    }

    std::map<std::string, MockPciSysfsDevice> devices;
    std::vector<std::string> deviceOrder;
    size_t largestConfigReadRequest = 0;
    uint32_t configOpenCallCount = 0;

  private:
    static inline MockPciSysfs *pMockPciSysfs = nullptr;

    struct OpenFile {
        std::string directoryName;
        std::string attribute;
    };

    static bool splitPath(const std::string &path, std::string &directoryName, std::string &attribute) {
        if (path.rfind(mockPciSysfsDevicesDirectory + "/", 0) != 0) {
            return false;
        }
        std::string remainder = path.substr(mockPciSysfsDevicesDirectory.size() + 1);
        auto separator = remainder.find('/');
        if (separator == std::string::npos) {
            return false;
        }
        directoryName = remainder.substr(0, separator);
        attribute = remainder.substr(separator + 1);
        return true;
    }

    static int mockOpen(const char *pathname, int flags) {
        std::string directoryName;
        std::string attribute;
        if ((pMockPciSysfs == nullptr) || !splitPath(pathname, directoryName, attribute)) {
            errno = ENOENT;
            return -1;
        }

        auto device = pMockPciSysfs->devices.find(directoryName);
        if (device == pMockPciSysfs->devices.end()) {
            errno = ENOENT;
            return -1;
        }

        if (attribute == "config") {
            pMockPciSysfs->configOpenCallCount++;
            if (device->second.configOpenErrorNum != 0) {
                errno = device->second.configOpenErrorNum;
                return -1;
            }
        } else if (device->second.attributes.find(attribute) == device->second.attributes.end()) {
            errno = ENOENT;
            return -1;
        }

        int fd = pMockPciSysfs->nextFd++;
        pMockPciSysfs->openFiles[fd] = {directoryName, attribute};
        return fd;
    }

    static ssize_t mockRead(int fd, void *buf, size_t count) {
        if (pMockPciSysfs == nullptr) {
            errno = EBADF;
            return -1;
        }
        auto openFile = pMockPciSysfs->openFiles.find(fd);
        if (openFile == pMockPciSysfs->openFiles.end()) {
            errno = EBADF;
            return -1;
        }
        auto &device = pMockPciSysfs->devices[openFile->second.directoryName];

        if (openFile->second.attribute == "config") {
            pMockPciSysfs->largestConfigReadRequest = std::max(pMockPciSysfs->largestConfigReadRequest, count);
            auto bytesToRead = std::min(static_cast<size_t>(std::max<ssize_t>(device.configBytesToRead, 0)), count);
            bytesToRead = std::min(bytesToRead, device.configHeader.size());
            memcpy(buf, device.configHeader.data(), bytesToRead);
            errno = device.configReadErrorNum;
            return device.configBytesToRead < 0 ? device.configBytesToRead : static_cast<ssize_t>(bytesToRead);
        }

        if (device.attributeReadErrorNum != 0) {
            errno = device.attributeReadErrorNum;
            return -1;
        }

        const auto &value = device.attributes[openFile->second.attribute];
        auto bytesToRead = std::min(count, value.size());
        memcpy(buf, value.data(), bytesToRead);
        return static_cast<ssize_t>(bytesToRead);
    }

    static int mockClose(int fd) {
        if (pMockPciSysfs != nullptr) {
            pMockPciSysfs->openFiles.erase(fd);
        }
        return 0;
    }

    static int mockAccess(const char *pathname, int mode) {
        std::string directoryName;
        std::string attribute;
        if ((pMockPciSysfs == nullptr) || !splitPath(pathname, directoryName, attribute) || (attribute != "physfn")) {
            errno = ENOENT;
            return -1;
        }
        auto device = pMockPciSysfs->devices.find(directoryName);
        if ((device == pMockPciSysfs->devices.end()) || !device->second.isVirtualFunction) {
            errno = ENOENT;
            return -1;
        }
        return 0;
    }

    std::map<int, OpenFile> openFiles;
    int nextFd = 100;

    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup{&NEO::directoryFilesMap};
    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> openBackup{&NEO::SysCalls::sysCallsOpen, &MockPciSysfs::mockOpen};
    VariableBackup<decltype(NEO::SysCalls::sysCallsRead)> readBackup{&NEO::SysCalls::sysCallsRead, &MockPciSysfs::mockRead};
    VariableBackup<decltype(NEO::SysCalls::sysCallsClose)> closeBackup{&NEO::SysCalls::sysCallsClose, &MockPciSysfs::mockClose};
    VariableBackup<decltype(NEO::SysCalls::sysCallsAccess)> accessBackup{&NEO::SysCalls::sysCallsAccess, &MockPciSysfs::mockAccess};
};

} // namespace ult
} // namespace Sysman
} // namespace L0
