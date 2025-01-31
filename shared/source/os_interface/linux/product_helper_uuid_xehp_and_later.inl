/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace UuidUtil {
static inline bool uuidReadFromTelem(std::string_view telemDir, std::array<char, PmtUtil::guidStringSize> &guidString,
                                     const uint64_t offset, const uint8_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid);
} // namespace UuidUtil

template <>
bool ProductHelperHw<gfxProduct>::getUuid(DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return false;
    }

    auto pDrm = driverModel->as<Drm>();
    std::optional<std::string> rootPciPath = getPciRootPath(pDrm->getFileDescriptor());
    if (!rootPciPath.has_value()) {
        return false;
    }
    std::map<uint32_t, std::string> telemPciPath;
    PmtUtil::getTelemNodesInPciPath(rootPciPath.value(), telemPciPath);
    // number of telem nodes must be same as subdevice count + 1(root device)
    if (telemPciPath.size() < subDeviceCount + 1) {
        return false;
    }

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
                       const uint8_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
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
