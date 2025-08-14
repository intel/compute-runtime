/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/os_compiler_cache_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/path.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_handle.h"
#include "shared/source/utilities/io_functions.h"

#include "os_inc.h"

#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string_view>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace NEO {
int filterFunction(const struct dirent *file) {
    std::string_view fileName = file->d_name;
    if (fileName.find(".cl_cache") != fileName.npos ||
        fileName.find(".l0_cache") != fileName.npos) {
        return 1;
    }

    return 0;
}

struct ElementsStruct {
    std::string path;
    struct stat statEl;
};

bool compareByLastAccessTime(const ElementsStruct &a, ElementsStruct &b) {
    return a.statEl.st_atime < b.statEl.st_atime;
}

bool CompilerCache::evictCache(uint64_t &bytesEvicted) {
    struct dirent **files = 0;

    const int filesCount = NEO::SysCalls::scandir(config.cacheDir.c_str(), &files, filterFunction, NULL);

    if (filesCount == -1) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Scandir failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
        return false;
    }

    std::vector<ElementsStruct> cacheFiles;
    cacheFiles.reserve(static_cast<size_t>(filesCount));
    for (int i = 0; i < filesCount; ++i) {
        ElementsStruct fileElement = {};
        fileElement.path = joinPath(config.cacheDir, files[i]->d_name);
        if (NEO::SysCalls::stat(fileElement.path.c_str(), &fileElement.statEl) == 0) {
            cacheFiles.push_back(std::move(fileElement));
        }
        free(files[i]);
    }

    free(files);

    std::sort(cacheFiles.begin(), cacheFiles.end(), compareByLastAccessTime);

    bytesEvicted = 0;
    const auto evictionLimit = config.cacheSize / 3;

    for (const auto &file : cacheFiles) {
        auto res = NEO::SysCalls::unlink(file.path);
        if (res == -1) {
            continue;
        }

        bytesEvicted += file.statEl.st_size;
        if (bytesEvicted > evictionLimit) {
            return true;
        }
    }

    return true;
}

bool CompilerCache::createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) {
    int fd = NEO::SysCalls::mkstemp(tmpFilePathTemplate);
    if (fd == -1) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
        return false;
    }
    if (NEO::SysCalls::pwrite(fd, pBinary, binarySize, 0) == -1) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
        NEO::SysCalls::close(fd);
        NEO::SysCalls::unlink(tmpFilePathTemplate);
        return false;
    }

    return NEO::SysCalls::close(fd) == 0;
}

bool CompilerCache::renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) {
    int err = NEO::SysCalls::rename(oldName.c_str(), kernelFileHash.c_str());

    if (err < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Rename temp file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
        NEO::SysCalls::unlink(oldName);
        return false;
    }

    return true;
}

void unlockFileAndClose(int fd) {
    int lockErr = NEO::SysCalls::flock(fd, LOCK_UN);

    if (lockErr < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: unlock file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
    }

    NEO::SysCalls::close(fd);
}

void CompilerCache::lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) {
    bool countDirectorySize = false;
    errno = 0;
    std::get<int>(fd) = NEO::SysCalls::open(configFilePath.c_str(), O_RDWR);

    if (std::get<int>(fd) < 0) {
        if (errno == ENOENT) {
            std::get<int>(fd) = NEO::SysCalls::openWithMode(configFilePath.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if (std::get<int>(fd) < 0) {
                std::get<int>(fd) = NEO::SysCalls::open(configFilePath.c_str(), O_RDWR);
            } else {
                countDirectorySize = true;
            }
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Open config file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
            return;
        }
    }

    const int lockErr = NEO::SysCalls::flock(std::get<int>(fd), LOCK_EX);

    if (lockErr < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Lock config file failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
        NEO::SysCalls::close(std::get<int>(fd));
        std::get<int>(fd) = -1;

        return;
    }

    if (countDirectorySize) {
        struct dirent **files = {};

        const int filesCount = NEO::SysCalls::scandir(config.cacheDir.c_str(), &files, filterFunction, NULL);

        if (filesCount == -1) {
            unlockFileAndClose(std::get<int>(fd));
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Scandir failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
            std::get<int>(fd) = -1;
            return;
        }

        std::vector<ElementsStruct> cacheFiles;
        cacheFiles.reserve(static_cast<size_t>(filesCount));
        for (int i = 0; i < filesCount; ++i) {
            std::string_view fileName = files[i]->d_name;
            if (fileName.find(config.cacheFileExtension) != fileName.npos) {
                ElementsStruct fileElement = {};
                fileElement.path = joinPath(config.cacheDir, files[i]->d_name);
                if (NEO::SysCalls::stat(fileElement.path.c_str(), &fileElement.statEl) == 0) {
                    cacheFiles.push_back(std::move(fileElement));
                }
            }
            free(files[i]);
        }

        free(files);

        for (const auto &element : cacheFiles) {
            directorySize += element.statEl.st_size;
        }

    } else {
        const ssize_t readErr = NEO::SysCalls::pread(std::get<int>(fd), &directorySize, sizeof(directorySize), 0);

        if (readErr < 0) {
            directorySize = 0;
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Read config failed! errno: %d\n", NEO::SysCalls::getProcessId(), errno);
            unlockFileAndClose(std::get<int>(fd));
            std::get<int>(fd) = -1;
        }
    }
}

class HandleGuard : NonCopyableAndNonMovableClass {
  public:
    HandleGuard() = delete;
    explicit HandleGuard(int &fileDescriptor) : fd(fileDescriptor) {}
    ~HandleGuard() {
        if (fd != -1) {
            unlockFileAndClose(fd);
        }
    }

  private:
    int fd = -1;
};

static_assert(NonCopyableAndNonMovable<HandleGuard>);

bool CompilerCache::cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize) {
    if (pBinary == nullptr || binarySize == 0 || binarySize > config.cacheSize) {
        return false;
    }

    std::unique_lock<std::mutex> lock(cacheAccessMtx);
    constexpr std::string_view configFileName = "config.file";

    std::string configFilePath = joinPath(config.cacheDir, configFileName.data());
    std::string cacheFilePath = joinPath(config.cacheDir, kernelFileHash + config.cacheFileExtension);

    UnifiedHandle fd{-1};
    size_t directorySize = 0u;

    lockConfigFileAndReadSize(configFilePath, fd, directorySize);

    if (std::get<int>(fd) < 0) {
        return false;
    }

    HandleGuard configGuard(std::get<int>(fd));

    struct stat statbuf = {};
    if (NEO::SysCalls::stat(cacheFilePath, &statbuf) == 0) {
        return true;
    }

    const size_t maxSize = config.cacheSize;
    if (maxSize < (directorySize + binarySize)) {
        uint64_t bytesEvicted{0u};
        const auto evictSuccess = evictCache(bytesEvicted);
        const auto availableSpace = maxSize - directorySize + bytesEvicted;

        directorySize = std::max<size_t>(0, directorySize - bytesEvicted);

        if (!evictSuccess || binarySize > availableSpace) {
            if (bytesEvicted > 0) {
                NEO::SysCalls::pwrite(std::get<int>(fd), &directorySize, sizeof(directorySize), 0);
            }
            return false;
        }
    }

    std::string tmpFileName = "cl_cache.XXXXXX";
    std::string tmpFilePath = joinPath(config.cacheDir, tmpFileName);

    if (!createUniqueTempFileAndWriteData(tmpFilePath.data(), pBinary, binarySize)) {
        return false;
    }

    if (!renameTempFileBinaryToProperName(tmpFilePath, cacheFilePath)) {
        return false;
    }

    directorySize += binarySize;

    NEO::SysCalls::pwrite(std::get<int>(fd), &directorySize, sizeof(directorySize), 0);

    return true;
}

std::unique_ptr<char[]> CompilerCache::loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize) {
    std::string filePath = joinPath(config.cacheDir, kernelFileHash + config.cacheFileExtension);

    return loadDataFromFile(filePath.c_str(), cachedBinarySize);
}
} // namespace NEO
