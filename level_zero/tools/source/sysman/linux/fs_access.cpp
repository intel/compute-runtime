/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include <climits>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>

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

// Generic Filesystem Access
FsAccess::FsAccess() {
}

FsAccess *FsAccess::create() {
    return new FsAccess();
}

ze_result_t FsAccess::read(const std::string file, std::string &val) {
    // Read a single line from text file without trailing newline
    std::ifstream fs;
    val.clear();

    fs.open(file.c_str());
    if (fs.fail()) {
        return getResult(errno);
    }
    fs >> val;
    if (fs.fail()) {
        fs.close();
        return getResult(errno);
    }
    fs.close();
    // Strip trailing newline
    if (val.back() == '\n') {
        val.pop_back();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::read(const std::string file, std::vector<std::string> &val) {
    // Read a entire text file, one line per vector entry
    std::string line;
    std::ifstream fs;
    val.clear();

    fs.open(file.c_str());
    if (fs.fail()) {
        return getResult(errno);
    }
    while (std::getline(fs, line)) {
        if (fs.fail()) {
            fs.close();
            return getResult(errno);
        }
        if (line.back() == '\n') {
            line.pop_back();
        }
        val.push_back(line);
    }
    fs.close();

    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::write(const std::string file, const std::string val) {
    std::ofstream sysfs;

    sysfs.open(file.c_str());
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

ze_result_t FsAccess::canRead(const std::string file) {
    if (access(file.c_str(), F_OK)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (access(file.c_str(), R_OK)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::canWrite(const std::string file) {
    if (access(file.c_str(), F_OK)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (access(file.c_str(), W_OK)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::readSymLink(const std::string path, std::string &val) {
    // returns the value of symlink at path
    char buf[PATH_MAX];
    ssize_t len = ::readlink(path.c_str(), buf, PATH_MAX - 1);
    if (len < 0) {
        return getResult(errno);
    }
    buf[len] = '\0';
    val = std::string(buf);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::getRealPath(const std::string path, std::string &val) {
    // returns the real file path after resolving all symlinks in path
    char buf[PATH_MAX];
    char *realPath = ::realpath(path.c_str(), buf);
    if (!realPath) {
        return getResult(errno);
    }
    val = std::string(buf);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccess::listDirectory(const std::string path, std::vector<std::string> &list) {
    list.clear();
    ::DIR *procDir = ::opendir(path.c_str());
    if (!procDir) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    struct ::dirent *ent;
    while (NULL != (ent = ::readdir(procDir))) {
        // Ignore . and ..
        std::string name = std::string(ent->d_name);
        if (!name.compare(".") || !name.compare("..")) {
            continue;
        }
        list.push_back(std::string(ent->d_name));
    }
    ::closedir(procDir);
    return ZE_RESULT_SUCCESS;
}

std::string FsAccess::getBaseName(const std::string path) {
    size_t pos = path.rfind("/");
    if (std::string::npos == pos) {
        return path;
    }
    return path.substr(pos + 1, std::string::npos);
}

std::string FsAccess::getDirName(const std::string path) {
    size_t pos = path.rfind("/");
    if (std::string::npos == pos) {
        return std::string("");
    }
    // Include trailing slash
    return path.substr(0, pos);
}

ze_bool_t FsAccess::fileExists(const std::string file) {
    struct stat sb;
    if (stat(file.c_str(), &sb) == 0) {
        return true;
    }
    return false;
}

// Procfs Access
const std::string ProcfsAccess::procDir = "/proc/";
const std::string ProcfsAccess::fdDir = "/fd/";

std::string ProcfsAccess::fullPath(const ::pid_t pid) {
    // Returns the full path for proc entry for process pid
    return std::string(procDir + std::to_string(pid));
}

std::string ProcfsAccess::fdDirPath(const ::pid_t pid) {
    // Returns the full path to file descritpor directory
    // for process pid
    return std::string(fullPath(pid) + fdDir);
}

std::string ProcfsAccess::fullFdPath(const ::pid_t pid, const int fd) {
    // Returns the full path for filedescriptor fd
    // for process pid
    return std::string(fdDirPath(pid) + std::to_string(fd));
}

ProcfsAccess *ProcfsAccess::create() {
    return new ProcfsAccess();
}

ze_result_t ProcfsAccess::listProcesses(std::vector<::pid_t> &list) {
    // Returns a vector with all the active process ids in the system
    list.clear();
    std::vector<std::string> dir;
    ze_result_t result = FsAccess::listDirectory(procDir, dir);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&file : dir) {
        ::pid_t pid;
        std::istringstream stream(file);
        stream >> pid;
        if (stream.fail()) {
            // Non numeric filename, not a process, skip
            continue;
        }
        list.push_back(pid);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ProcfsAccess::getFileDescriptors(const ::pid_t pid, std::vector<int> &list) {
    // Returns a vector with all the filedescriptor numbers opened by a pid
    list.clear();
    std::vector<std::string> dir;
    ze_result_t result = FsAccess::listDirectory(fdDirPath(pid), dir);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&file : dir) {
        int fd;
        std::istringstream stream(file);
        stream >> fd;
        if (stream.fail()) {
            // Non numeric filename, not a file descriptor
            continue;
        }
        list.push_back(fd);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ProcfsAccess::getFileName(const ::pid_t pid, const int fd, std::string &val) {
    // Given a process id and a file descriptor number
    // return full name of the open file.
    // NOTE: For sockets, the name will be of the format "socket:[nnnnnnn]"
    return FsAccess::readSymLink(fullFdPath(pid, fd), val);
}

::pid_t ProcfsAccess::myProcessId() {
    return ::getpid();
}

// Sysfs Access
const std::string SysfsAccess::drmPath = "/sys/class/drm/";
const std::string SysfsAccess::devicesPath = "device/drm/";
const std::string SysfsAccess::primaryDevName = "card";
const std::string SysfsAccess::drmDriverDevNodeDir = "/dev/dri/";
const std::string SysfsAccess::intelGpuBindEntry = "/sys/bus/pci/drivers/i915/bind";
const std::string SysfsAccess::intelGpuUnbindEntry = "/sys/bus/pci/drivers/i915/unbind";

std::string SysfsAccess::fullPath(const std::string file) {
    // Prepend sysfs directory path for this device
    return std::string(dirname + file);
}

SysfsAccess::SysfsAccess(const std::string dev) {
    // dev could be either /dev/dri/cardX or /dev/dri/renderDX
    std::string fileName = FsAccess::getBaseName(dev);
    std::string devicesDir = drmPath + fileName + std::string("/") + devicesPath;

    FsAccess::listDirectory(devicesDir, deviceNames);
    for (auto &&next : deviceNames) {
        if (!next.compare(0, primaryDevName.length(), primaryDevName)) {
            dirname = drmPath + next + std::string("/");
            break;
        }
    }
}

SysfsAccess *SysfsAccess::create(const std::string dev) {
    return new SysfsAccess(dev);
}

ze_result_t SysfsAccess::canRead(const std::string file) {
    // Prepend sysfs directory path and call the base canRead
    return FsAccess::canRead(fullPath(file));
}

ze_result_t SysfsAccess::canWrite(const std::string file) {
    // Prepend sysfs directory path and call the base canWrite
    return FsAccess::canWrite(fullPath(file));
}

ze_result_t SysfsAccess::read(const std::string file, std::string &val) {
    // Prepend sysfs directory path and call the base read
    return FsAccess::read(fullPath(file).c_str(), val);
}

ze_result_t SysfsAccess::read(const std::string file, int &val) {
    std::string str;
    ze_result_t result;

    result = FsAccess::read(fullPath(file), str);
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

    result = FsAccess::read(fullPath(file), str);
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

ze_result_t SysfsAccess::read(const std::string file, std::vector<std::string> &val) {
    // Prepend sysfs directory path and call the base read
    return FsAccess::read(fullPath(file), val);
}

ze_result_t SysfsAccess::write(const std::string file, const std::string val) {
    // Prepend sysfs directory path and call the base write
    return FsAccess::write(fullPath(file).c_str(), val);
}

ze_result_t SysfsAccess::write(const std::string file, const int val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return FsAccess::write(fullPath(file), stream.str());
}

ze_result_t SysfsAccess::write(const std::string file, const double val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return FsAccess::write(fullPath(file), stream.str());
}

ze_result_t SysfsAccess::readSymLink(const std::string path, std::string &val) {
    // Prepend sysfs directory path and call the base readSymLink
    return FsAccess::readSymLink(fullPath(path).c_str(), val);
}

ze_result_t SysfsAccess::getRealPath(const std::string path, std::string &val) {
    // Prepend sysfs directory path and call the base getRealPath
    return FsAccess::getRealPath(fullPath(path).c_str(), val);
}

ze_result_t SysfsAccess::bindDevice(std::string device) {
    return FsAccess::write(intelGpuBindEntry, device);
}

ze_result_t SysfsAccess::unbindDevice(std::string device) {
    return FsAccess::write(intelGpuUnbindEntry, device);
}

ze_bool_t SysfsAccess::fileExists(const std::string file) {
    // Prepend sysfs directory path and call the base fileExists
    return FsAccess::fileExists(fullPath(file).c_str());
}

ze_bool_t SysfsAccess::isMyDeviceFile(const std::string dev) {
    // dev is a full pathname.
    if (getDirName(dev).compare(drmDriverDevNodeDir)) {
        for (auto &&next : deviceNames) {
            if (!getBaseName(dev).compare(next)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace L0
