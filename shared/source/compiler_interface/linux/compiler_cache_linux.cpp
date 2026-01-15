/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/path.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_handle.h"

#include "elements_struct.h"

#include <algorithm>
#include <dirent.h>
#include <fts.h>
#include <string_view>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

namespace NEO {
bool CompilerCache::compareByLastAccessTime(const ElementsStruct &a, const ElementsStruct &b) {
    return a.lastAccessTime < b.lastAccessTime;
}

bool CompilerCache::createCacheDirectories(const std::string &cacheFile) {
    std::string path = config.cacheDir;
    for (int i = 0; i < maxCacheDepth; i++) {
        path = joinPath(path, std::string(1, cacheFile[i]));
        if (NEO::SysCalls::mkdir(path)) {
            int error = errno;
            if (error != EEXIST) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating cache directories failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
                return false;
            }
        }
    }

    return true;
}

bool CompilerCache::getFiles(const std::string &startPath, const std::function<bool(const std::string_view &)> &filter, std::vector<ElementsStruct> &foundFiles) {
    foundFiles.clear();
    auto dirCopy{startPath};
    char *dirPath[2] = {dirCopy.data(), nullptr};
    FTS *fts = NEO::SysCalls::ftsOpen(dirPath, FTS_PHYSICAL | FTS_NOCHDIR, nullptr);
    if (fts == nullptr) {
        int error = errno;
        if (error != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [findFiles failure]: Opening directory %s failed! errno: %d\n", NEO::SysCalls::getProcessId(), startPath.c_str(), error);
            return false;
        }
        return true;
    }

    FTSENT *ent;
    while ((ent = NEO::SysCalls::ftsRead(fts)) != nullptr) {
        if (ent->fts_info == FTS_ERR) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [findFiles failure]: Reading file failed! fts_errno: %d\n", NEO::SysCalls::getProcessId(), ent->fts_errno);
            NEO::SysCalls::ftsClose(fts);
            return false;
        }
        if (ent->fts_info == FTS_F && filter(ent->fts_path)) {
            ElementsStruct fileElement = {};
            fileElement.path = std::string(ent->fts_path);
            fileElement.lastAccessTime = ent->fts_statp->st_atime;
            fileElement.fileSize = ent->fts_statp->st_size;
            foundFiles.push_back(fileElement);
        }
    }
    int error = errno;
    if (error != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [findFiles failure]: Reading file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
        NEO::SysCalls::ftsClose(fts);
        return false;
    }

    NEO::SysCalls::ftsClose(fts);
    return true;
}

bool CompilerCache::evictCache(uint64_t &bytesEvicted) {
    std::vector<ElementsStruct> cacheFiles;
    if (!getCachedFiles(cacheFiles)) {
        return false;
    }

    bytesEvicted = 0;
    const auto evictionLimit = config.cacheSize / 3;

    for (const auto &file : cacheFiles) {
        auto res = NEO::SysCalls::unlink(file.path);
        if (res == -1) {
            continue;
        }

        bytesEvicted += file.fileSize;
        if (bytesEvicted > evictionLimit) {
            return true;
        }
    }

    return true;
}

bool CompilerCache::createUniqueTempFileAndWriteData(char *tmpFilePathTemplate, const char *pBinary, size_t binarySize) {
    int fd = NEO::SysCalls::mkstemp(tmpFilePathTemplate);
    if (fd == -1) {
        int error = errno;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
        return false;
    }
    if (NEO::SysCalls::pwrite(fd, pBinary, binarySize, 0) == -1) {
        int error = errno;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
        NEO::SysCalls::close(fd);
        NEO::SysCalls::unlink(tmpFilePathTemplate);
        return false;
    }

    return NEO::SysCalls::close(fd) == 0;
}

bool CompilerCache::renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) {
    int err = NEO::SysCalls::rename(oldName.c_str(), kernelFileHash.c_str());

    if (err < 0) {
        int error = errno;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Rename temp file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
        return false;
    }

    return true;
}

void unlockFileAndClose(int fd) {
    int lockErr = NEO::SysCalls::flock(fd, LOCK_UN);

    if (lockErr < 0) {
        int error = errno;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: unlock file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
    }

    NEO::SysCalls::close(fd);
}

void CompilerCache::lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &fd, size_t &directorySize) {
    bool countDirectorySize = false;
    errno = 0;
    std::get<int>(fd) = NEO::SysCalls::open(configFilePath.c_str(), O_RDWR);

    if (std::get<int>(fd) < 0) {
        int error = errno;
        if (error == ENOENT) {
            std::get<int>(fd) = NEO::SysCalls::openWithMode(configFilePath.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if (std::get<int>(fd) < 0) {
                std::get<int>(fd) = NEO::SysCalls::open(configFilePath.c_str(), O_RDWR);
            } else {
                countDirectorySize = true;
            }
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Open config file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
            return;
        }
    }

    const int lockErr = NEO::SysCalls::flock(std::get<int>(fd), LOCK_EX);

    if (lockErr < 0) {
        int error = errno;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Lock config file failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
        NEO::SysCalls::close(std::get<int>(fd));
        std::get<int>(fd) = -1;

        return;
    }

    if (countDirectorySize) {
        std::vector<ElementsStruct> cacheFiles;
        if (!getCachedFiles(cacheFiles)) {
            unlockFileAndClose(std::get<int>(fd));
            std::get<int>(fd) = -1;
            return;
        }

        for (const auto &element : cacheFiles) {
            directorySize += element.fileSize;
        }

    } else {
        const ssize_t readErr = NEO::SysCalls::pread(std::get<int>(fd), &directorySize, sizeof(directorySize), 0);

        if (readErr < 0) {
            directorySize = 0;
            int error = errno;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Read config failed! errno: %d\n", NEO::SysCalls::getProcessId(), error);
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
    std::string cacheFileName = kernelFileHash + config.cacheFileExtension;
    std::string cacheFilePath = getCachedFilePath(cacheFileName);

    if (cacheFileName.length() < maxCacheDepth + config.cacheFileExtension.length()) {
        DEBUG_BREAK_IF(true);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Cache binary failed - cache file name is too short!\n", NEO::SysCalls::getProcessId());
        return false;
    }

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

    if (!createCacheDirectories(cacheFileName)) {
        NEO::SysCalls::unlink(tmpFilePath.c_str());
        return false;
    }

    if (!renameTempFileBinaryToProperName(tmpFilePath, cacheFilePath)) {
        NEO::SysCalls::unlink(tmpFilePath.c_str());
        return false;
    }

    directorySize += binarySize;

    NEO::SysCalls::pwrite(std::get<int>(fd), &directorySize, sizeof(directorySize), 0);

    return true;
}
} // namespace NEO
