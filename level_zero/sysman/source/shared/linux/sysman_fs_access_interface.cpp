/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace L0 {
namespace Sysman {

FsAccessInterface::~FsAccessInterface() = default;

void FdCacheInterface::eraseLeastUsedEntryFromCache() {
    auto it = fdMap.begin();
    uint32_t fdCountRef = it->second.second;
    std::map<std::string, std::pair<int, uint32_t>>::iterator leastUsedIterator = it;
    while (++it != fdMap.end()) {
        if (it->second.second < fdCountRef) {
            fdCountRef = it->second.second;
            leastUsedIterator = it;
        }
    }
    NEO::SysCalls::close(leastUsedIterator->second.first);
    fdMap.erase(leastUsedIterator);
}

int FdCacheInterface::getFd(std::string file) {
    int fd = -1;
    if (fdMap.find(file) == fdMap.end()) {
        fd = NEO::SysCalls::open(file.c_str(), O_RDONLY);
        if (fd < 0) {
            return -1;
        }
        if (fdMap.size() == maxSize) {
            eraseLeastUsedEntryFromCache();
        }
        fdMap[file] = std::make_pair(fd, 1);
    } else {
        auto &fdPair = fdMap[file];
        fdPair.second++;
    }
    return fdMap[file].first;
}

FdCacheInterface::~FdCacheInterface() {
    for (auto it = fdMap.begin(); it != fdMap.end(); ++it) {
        NEO::SysCalls::close(it->second.first);
    }
    fdMap.clear();
}

template <typename T>
ze_result_t FsAccessInterface::readValue(const std::string file, T &val) {
    auto lock = this->obtainMutex();

    std::string readVal(64, '\0');
    int fd = pFdCacheInterface->getFd(file);
    if (fd < 0) {
        return LinuxSysmanImp::getResult(errno);
    }

    ssize_t bytesRead = NEO::SysCalls::pread(fd, readVal.data(), readVal.size(), 0);
    if (bytesRead < 0) {
        return LinuxSysmanImp::getResult(errno);
    }

    std::istringstream stream(readVal);
    stream >> val;
    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

// Generic Filesystem Access
FsAccessInterface::FsAccessInterface() {
    pFdCacheInterface = std::make_unique<FdCacheInterface>();
}

std::unique_ptr<FsAccessInterface> FsAccessInterface::create() {
    return std::unique_ptr<FsAccessInterface>(new FsAccessInterface());
}

ze_result_t FsAccessInterface::read(const std::string file, uint64_t &val) {
    return readValue<uint64_t>(file, val);
}

ze_result_t FsAccessInterface::read(const std::string file, double &val) {
    return readValue<double>(file, val);
}

ze_result_t FsAccessInterface::read(const std::string file, int32_t &val) {
    return readValue<int32_t>(file, val);
}

ze_result_t FsAccessInterface::read(const std::string file, uint32_t &val) {
    return readValue<uint32_t>(file, val);
}

ze_result_t FsAccessInterface::read(const std::string file, std::string &val) {
    // Read a single line from text file without trailing newline
    std::ifstream fs;
    val.clear();

    fs.open(file.c_str());
    if (fs.fail()) {
        return LinuxSysmanImp::getResult(errno);
    }
    fs >> val;
    if (fs.fail()) {
        fs.close();
        return LinuxSysmanImp::getResult(errno);
    }
    fs.close();
    // Strip trailing newline
    if (val.back() == '\n') {
        val.pop_back();
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::read(const std::string file, std::vector<std::string> &val) {
    // Read a entire text file, one line per vector entry
    std::string line;
    std::ifstream fs;
    val.clear();

    fs.open(file.c_str());
    if (fs.fail()) {
        return LinuxSysmanImp::getResult(errno);
    }
    while (std::getline(fs, line)) {
        if (fs.fail()) {
            fs.close();
            return LinuxSysmanImp::getResult(errno);
        }
        if (line.back() == '\n') {
            line.pop_back();
        }
        val.push_back(line);
    }
    fs.close();

    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::write(const std::string file, const std::string val) {
    int fd = NEO::SysCalls::open(file.c_str(), O_WRONLY);
    if (fd < 0) {
        return LinuxSysmanImp::getResult(errno);
    }

    ssize_t bytesWritten = NEO::SysCalls::pwrite(fd, val.data(), val.size(), 0);
    NEO::SysCalls::close(fd);
    if (bytesWritten < 0) {
        return LinuxSysmanImp::getResult(errno);
    }

    if (bytesWritten != static_cast<ssize_t>(val.size())) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::canRead(const std::string file) {
    struct stat sb;
    if (NEO::SysCalls::stat(file.c_str(), &sb) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (sb.st_mode & S_IRUSR) {
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
}

ze_result_t FsAccessInterface::canWrite(const std::string file) {
    struct stat sb;
    if (NEO::SysCalls::stat(file.c_str(), &sb) != 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    if (sb.st_mode & S_IWUSR) {
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
}

bool FsAccessInterface::fileExists(const std::string file) {
    struct stat sb;

    if (NEO::SysCalls::stat(file.c_str(), &sb) != 0) {
        return false;
    }

    if (!S_ISREG(sb.st_mode)) {
        return false;
    }

    if (NEO::SysCalls::access(file.c_str(), F_OK)) {
        return false;
    }

    return true;
}

ze_result_t FsAccessInterface::getFileMode(const std::string file, ::mode_t &mode) {
    struct stat sb;
    if (0 != NEO::SysCalls::stat(file.c_str(), &sb)) {
        return LinuxSysmanImp::getResult(errno);
    }
    mode = sb.st_mode;
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::readSymLink(const std::string path, std::string &val) {
    // returns the value of symlink at path
    char buf[PATH_MAX];
    ssize_t len = NEO::SysCalls::readlink(path.c_str(), buf, PATH_MAX - 1);
    if (len < 0) {
        return LinuxSysmanImp::getResult(errno);
    }
    buf[len] = '\0';
    val = std::string(buf);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::getRealPath(const std::string path, std::string &val) {
    // returns the real file path after resolving all symlinks in path
    char buf[PATH_MAX];
    char *realPath = NEO::SysCalls::realpath(path.c_str(), buf);
    if (!realPath) {
        return LinuxSysmanImp::getResult(errno);
    }
    val = std::string(buf);
    return ZE_RESULT_SUCCESS;
}

ze_result_t FsAccessInterface::listDirectory(const std::string path, std::vector<std::string> &list) {
    list.clear();
    ::DIR *procDir = NEO::SysCalls::opendir(path.c_str());
    if (!procDir) {
        return LinuxSysmanImp::getResult(errno);
    }
    struct ::dirent *ent;
    int err = 0;
    // readdir doesn't clear errno, so make sure it is clear
    errno = 0;
    while (NULL != (ent = NEO::SysCalls::readdir(procDir))) {
        // Ignore . and ..
        std::string name = std::string(ent->d_name);
        if (!name.compare(".") || !name.compare("..")) {
            errno = 0;
            continue;
        }
        list.push_back(std::string(ent->d_name));
        errno = 0;
    }
    err = errno;
    NEO::SysCalls::closedir(procDir);
    // Check if in above while loop, readdir encountered any error.
    if ((err != 0) && (err != ENOENT)) {
        list.clear();
        return LinuxSysmanImp::getResult(err);
    }
    return ZE_RESULT_SUCCESS;
}

std::string FsAccessInterface::getBaseName(const std::string path) {
    size_t pos = path.rfind("/");
    if (std::string::npos == pos) {
        return path;
    }
    return path.substr(pos + 1, std::string::npos);
}

std::string FsAccessInterface::getDirName(const std::string path) {
    size_t pos = path.rfind("/");
    if (std::string::npos == pos) {
        return std::string("");
    }
    // Include trailing slash
    return path.substr(0, pos);
}

bool FsAccessInterface::isRootUser() {
    return (geteuid() == 0);
}

bool FsAccessInterface::directoryExists(const std::string path) {
    if (NEO::SysCalls::access(path.c_str(), F_OK)) {
        return false;
    }
    return true;
}

std::unique_lock<std::mutex> FsAccessInterface::obtainMutex() {
    return std::unique_lock<std::mutex>(this->fsMutex);
}

// Procfs Access
ProcFsAccessInterface::ProcFsAccessInterface() = default;
ProcFsAccessInterface::~ProcFsAccessInterface() = default;

const std::string ProcFsAccessInterface::procDir = "/proc/";
const std::string ProcFsAccessInterface::fdDir = "/fd/";

std::string ProcFsAccessInterface::fullPath(const ::pid_t pid) {
    // Returns the full path for proc entry for process pid
    return std::string(procDir + std::to_string(pid));
}

std::string ProcFsAccessInterface::fdDirPath(const ::pid_t pid) {
    // Returns the full path to file descritpor directory
    // for process pid
    return std::string(fullPath(pid) + fdDir);
}

std::string ProcFsAccessInterface::fullFdPath(const ::pid_t pid, const int fd) {
    // Returns the full path for filedescriptor fd
    // for process pid
    return std::string(fdDirPath(pid) + std::to_string(fd));
}

std::unique_ptr<ProcFsAccessInterface> ProcFsAccessInterface::create() {
    return std::unique_ptr<ProcFsAccessInterface>(new ProcFsAccessInterface());
}

ze_result_t ProcFsAccessInterface::listProcesses(std::vector<::pid_t> &list) {
    // Returns a vector with all the active process ids in the system
    list.clear();
    std::vector<std::string> dir;
    ze_result_t result = FsAccessInterface::listDirectory(procDir, dir);
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

ze_result_t ProcFsAccessInterface::getFileDescriptors(const ::pid_t pid, std::vector<int> &list) {
    // Returns a vector with all the filedescriptor numbers opened by a pid
    list.clear();
    std::vector<std::string> dir;
    ze_result_t result = FsAccessInterface::listDirectory(fdDirPath(pid), dir);
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

ze_result_t ProcFsAccessInterface::getFileName(const ::pid_t pid, const int fd, std::string &val) {
    // Given a process id and a file descriptor number
    // return full name of the open file.
    // NOTE: For sockets, the name will be of the format "socket:[nnnnnnn]"
    return FsAccessInterface::readSymLink(fullFdPath(pid, fd), val);
}

bool ProcFsAccessInterface::isAlive(const ::pid_t pid) {
    return FsAccessInterface::fileExists(fullPath(pid));
}

void ProcFsAccessInterface::kill(const ::pid_t pid) {
    ::kill(pid, SIGKILL);
}

::pid_t ProcFsAccessInterface::myProcessId() {
    return ::getpid();
}

// Sysfs Access
SysFsAccessInterface::SysFsAccessInterface() = default;
SysFsAccessInterface::~SysFsAccessInterface() = default;

const std::string SysFsAccessInterface::drmPath = "/sys/class/drm/";
const std::string SysFsAccessInterface::devicesPath = "device/drm/";
const std::string SysFsAccessInterface::primaryDevName = "card";
const std::string SysFsAccessInterface::drmDriverDevNodeDir = "/dev/dri/";

std::string SysFsAccessInterface::fullPath(const std::string file) {
    // Prepend sysfs directory path for this device
    return std::string(dirname + file);
}

SysFsAccessInterface::SysFsAccessInterface(const std::string dev) {
    // dev could be either /dev/dri/cardX or /dev/dri/renderDX
    std::string fileName = FsAccessInterface::getBaseName(dev);
    std::string devicesDir = drmPath + fileName + std::string("/") + devicesPath;

    FsAccessInterface::listDirectory(devicesDir, deviceNames);
    for (auto &&next : deviceNames) {
        if (!next.compare(0, primaryDevName.length(), primaryDevName)) {
            dirname = drmPath + next + std::string("/");
            break;
        }
    }
}

std::unique_ptr<SysFsAccessInterface> SysFsAccessInterface::create(const std::string dev) {
    return std::unique_ptr<SysFsAccessInterface>(new SysFsAccessInterface(dev));
}

ze_result_t SysFsAccessInterface::canRead(const std::string file) {
    // Prepend sysfs directory path and call the base canRead
    return FsAccessInterface::canRead(fullPath(file));
}

ze_result_t SysFsAccessInterface::canWrite(const std::string file) {
    // Prepend sysfs directory path and call the base canWrite
    return FsAccessInterface::canWrite(fullPath(file));
}

ze_result_t SysFsAccessInterface::getFileMode(const std::string file, ::mode_t &mode) {
    // Prepend sysfs directory path and call the base getFileMode
    return FsAccessInterface::getFileMode(fullPath(file), mode);
}

ze_result_t SysFsAccessInterface::read(const std::string file, std::string &val) {
    // Prepend sysfs directory path and call the base read
    return FsAccessInterface::read(fullPath(file).c_str(), val);
}

ze_result_t SysFsAccessInterface::read(const std::string file, int32_t &val) {
    return FsAccessInterface::read(fullPath(file), val);
}

ze_result_t SysFsAccessInterface::read(const std::string file, uint32_t &val) {
    return FsAccessInterface::read(fullPath(file), val);
}

ze_result_t SysFsAccessInterface::read(const std::string file, double &val) {
    return FsAccessInterface::read(fullPath(file), val);
}

ze_result_t SysFsAccessInterface::read(const std::string file, uint64_t &val) {
    return FsAccessInterface::read(fullPath(file), val);
}

ze_result_t SysFsAccessInterface::read(const std::string file, std::vector<std::string> &val) {
    // Prepend sysfs directory path and call the base read
    return FsAccessInterface::read(fullPath(file), val);
}

ze_result_t SysFsAccessInterface::write(const std::string file, const std::string val) {
    // Prepend sysfs directory path and call the base write
    return FsAccessInterface::write(fullPath(file).c_str(), val);
}

ze_result_t SysFsAccessInterface::write(const std::string file, const int val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return FsAccessInterface::write(fullPath(file), stream.str());
}

ze_result_t SysFsAccessInterface::write(const std::string file, const double val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return FsAccessInterface::write(fullPath(file), stream.str());
}

ze_result_t SysFsAccessInterface::write(const std::string file, const uint64_t val) {
    std::ostringstream stream;
    stream << val;

    if (stream.fail()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return FsAccessInterface::write(fullPath(file), stream.str());
}

ze_result_t SysFsAccessInterface::scanDirEntries(const std::string path, std::vector<std::string> &list) {
    list.clear();
    return FsAccessInterface::listDirectory(fullPath(path).c_str(), list);
}

ze_result_t SysFsAccessInterface::readSymLink(const std::string path, std::string &val) {
    // Prepend sysfs directory path and call the base readSymLink
    return FsAccessInterface::readSymLink(fullPath(path).c_str(), val);
}

ze_result_t SysFsAccessInterface::getRealPath(const std::string path, std::string &val) {
    // Prepend sysfs directory path and call the base getRealPath
    return FsAccessInterface::getRealPath(fullPath(path).c_str(), val);
}

ze_result_t SysFsAccessInterface::bindDevice(const std::string &gpuBindEntry, const std::string &device) {
    return FsAccessInterface::write(gpuBindEntry, device);
}

ze_result_t SysFsAccessInterface::unbindDevice(const std::string &gpuUnbindEntry, const std::string &device) {
    return FsAccessInterface::write(gpuUnbindEntry, device);
}

bool SysFsAccessInterface::fileExists(const std::string file) {
    // Prepend sysfs directory path and call the base fileExists
    return FsAccessInterface::fileExists(fullPath(file).c_str());
}

bool SysFsAccessInterface::directoryExists(const std::string path) {
    return FsAccessInterface::directoryExists(fullPath(path).c_str());
}

bool SysFsAccessInterface::isMyDeviceFile(const std::string dev) {
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

bool SysFsAccessInterface::isRootUser() {
    return FsAccessInterface::isRootUser();
}

} // namespace Sysman
} // namespace L0
