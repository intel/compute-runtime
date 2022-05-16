/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace UuidUtil {
static inline bool uuidReadFromTelem(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString,
                                     const uint64_t offset, const uint8_t deviceIndex, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid);
} // namespace UuidUtil

template <>
bool HwInfoConfigHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {

    UNRECOVERABLE_IF(device == nullptr);
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return false;
    }

    auto pDrm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<Drm>();
    std::optional<std::string> rootPciPath = getPciRootPath(pDrm->getFileDescriptor());
    if (!rootPciPath.has_value()) {
        return false;
    }

    std::map<uint32_t, std::string> telemPciPath;
    PmtUtil::getTelemNodesInPciPath(rootPciPath.value(), telemPciPath);

    // number of telem nodes must be same as subdevice count + 1(root device)
    if (telemPciPath.size() < device->getRootDevice()->getNumSubDevices() + 1) {
        return false;
    }

    auto deviceIndex = device->isSubDevice() ? static_cast<SubDevice *>(device)->getSubDeviceIndex() + 1 : 0;
    auto iterator = telemPciPath.begin();
    // use the root node
    std::string telemDir = iterator->second;

    std::array<char, PmtUtil::guidStringSize> guidString;
    if (!PmtUtil::readGuid(telemDir, guidString)) {
        return false;
    }

    uint64_t offset = ULONG_MAX;
    if (!PmtUtil::readOffset(telemDir, offset)) {
        return false;
    }

    return UuidUtil::uuidReadFromTelem(telemDir, guidString, offset, deviceIndex, uuid);
}

namespace UuidUtil {

bool uuidReadFromTelem(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString, const uint64_t offset,
                       const uint8_t deviceIndex, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) {
    auto pos = guidUuidOffsetMap.find(guidString.data());
    if (pos != guidUuidOffsetMap.end()) {
        uuid.fill(0);
        ssize_t bytesRead = PmtUtil::readTelem(telemDir.data(), pos->second.second, pos->second.first + offset, uuid.data());
        if (bytesRead == pos->second.second) {
            uuid[15] = deviceIndex;
            return true;
        }
    }
    return false;
}

} // namespace UuidUtil
