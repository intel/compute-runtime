/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysfs_access.h"

#include <climits>

#include <cerrno>
#include <cstdio>

namespace L0 {

static ze_result_t getResult(int err) {
    if ((EPERM == err) || (EACCES == err)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    } else if (ENOENT == err) {
        return ZE_RESULT_ERROR_UNKNOWN;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

const std::string SysfsAccess::charDevPath = "/sys/dev/char/";

std::string SysfsAccess::fullPath(const std::string file) {
    return std::string(dirname + file);
}

SysfsAccess::SysfsAccess(const int maj, const int min) {
    dirname = charDevPath + std::to_string(maj) + ":" + std::to_string(min) + "/";
}

SysfsAccess::SysfsAccess(const std::string path) {
    dirname = path;
}

SysfsAccess *SysfsAccess::create(const int fd) {
    struct stat st;

    if (fd < 0 || fstat(fd, &st) || !S_ISCHR(st.st_mode)) {
        return NULL;
    }

    return new SysfsAccess(std::string("/sys/class/drm/card0/"));
}

SysfsAccess *SysfsAccess::create(const std::string path) {
    return new SysfsAccess(path);
}

ze_result_t SysfsAccess::readAsString(const std::string file, std::string &val) {
    std::ifstream sysfs;
    val.clear();

    sysfs.open(fullPath(file).c_str());
    if (sysfs.fail()) {
        return getResult(errno);
    }
    sysfs >> val;
    if (sysfs.fail()) {
        sysfs.close();
        return getResult(errno);
    }
    sysfs.close();

    if (val.back() == '\n') {
        val.pop_back();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::readAsLines(const std::string file, std::vector<std::string> &val) {
    std::string line;
    std::ifstream sysfs;
    val.clear();

    sysfs.open(fullPath(file).c_str());
    if (sysfs.fail()) {
        return getResult(errno);
    }
    while (std::getline(sysfs, line)) {
        if (sysfs.fail()) {
            sysfs.close();
            return getResult(errno);
        }
        if (line.back() == '\n') {
            line.pop_back();
        }
        val.push_back(line);
    }
    sysfs.close();

    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::writeAsString(const std::string file, const std::string val) {
    std::ofstream sysfs;

    sysfs.open(fullPath(file).c_str());
    if (sysfs.fail()) {
        return getResult(errno);
    }
    sysfs << val << std::endl;
    if (sysfs.fail()) {
        sysfs.close();
        return getResult(errno);
    }
    sysfs.close();
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::canRead(const std::string file) {
    if (access(fullPath(file).c_str(), F_OK)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (access(fullPath(file).c_str(), R_OK)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::canWrite(const std::string file) {
    if (access(fullPath(file).c_str(), F_OK)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (access(fullPath(file).c_str(), W_OK)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::read(const std::string file, int &val) {
    std::string str;
    ze_result_t result;

    result = readAsString(file, str);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    std::istringstream stream(str);
    stream >> val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::read(const std::string file, double &val) {
    std::string str;
    ze_result_t result;

    result = readAsString(file, str);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    std::istringstream stream(str);
    stream >> val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysfsAccess::write(const std::string file, const int val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return writeAsString(file, stream.str());
}

ze_result_t SysfsAccess::write(const std::string file, const double val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return writeAsString(file, stream.str());
}

ze_result_t SysfsAccess::readSymLink(const std::string path, std::string &val) {
    char buf[PATH_MAX];
    ssize_t len = ::readlink(fullPath(path).c_str(), buf, PATH_MAX - 1);
    if (len < 0) {
        return getResult(errno);
    }
    buf[len] = '\0';
    val = std::string(buf);
    return ZE_RESULT_SUCCESS;
}

ze_bool_t SysfsAccess::fileExists(const std::string file) {
    struct stat sb;
    if (stat(fullPath(file).c_str(), &sb) == 0) {
        return true;
    }

    return false;
}

} // namespace L0
