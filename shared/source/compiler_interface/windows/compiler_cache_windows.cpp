/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/path.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/utilities/io_functions.h"

#include "os_inc.h"

#include <algorithm>

namespace NEO {

struct ElementsStruct {
    std::string path;
    FILETIME lastAccessTime;
    uint64_t fileSize;
};

std::vector<ElementsStruct> getFiles(const std::string &path) {
    std::vector<ElementsStruct> files;

    WIN32_FIND_DATAA ffd{0};
    HANDLE hFind = INVALID_HANDLE_VALUE;

    std::string newPath = joinPath(path, "*");
    hFind = NEO::SysCalls::findFirstFileA(newPath.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: File search failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return files;
    }

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }

        auto fileName = joinPath(path, ffd.cFileName);
        if (fileName.find(".cl_cache") != fileName.npos ||
            fileName.find(".l0_cache") != fileName.npos) {
            uint64_t fileSize = (ffd.nFileSizeHigh * (MAXDWORD + 1)) + ffd.nFileSizeLow;
            files.push_back({fileName, ffd.ftLastAccessTime, fileSize});
        }
    } while (NEO::SysCalls::findNextFileA(hFind, &ffd) != 0);

    NEO::SysCalls::findClose(hFind);

    std::sort(files.begin(), files.end(), [](const ElementsStruct &a, const ElementsStruct &b) { return CompareFileTime(&a.lastAccessTime, &b.lastAccessTime) < 0; });

    return files;
}

void unlockFileAndClose(UnifiedHandle handle) {
    OVERLAPPED overlapped = {0};
    auto result = NEO::SysCalls::unlockFileEx(std::get<void *>(handle), 0, MAXDWORD, MAXDWORD, &overlapped);

    if (!result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Unlock file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    }

    NEO::SysCalls::closeHandle(std::get<void *>(handle));
}

bool CompilerCache::evictCache(uint64_t &bytesEvicted) {
    bytesEvicted = 0;
    const auto cacheFiles = getFiles(config.cacheDir);
    const auto evictionLimit = config.cacheSize / 3;

    for (const auto &file : cacheFiles) {
        auto res = NEO::SysCalls::deleteFileA(file.path.c_str());
        if (!res) {
            continue;
        }

        bytesEvicted += file.fileSize;
        if (bytesEvicted > evictionLimit) {
            return true;
        }
    }

    return true;
}

void CompilerCache::lockConfigFileAndReadSize(const std::string &configFilePath, UnifiedHandle &handle, size_t &directorySize) {
    bool countDirectorySize = false;

    std::get<void *>(handle) = NEO::SysCalls::createFileA(configFilePath.c_str(),
                                                          GENERIC_READ | GENERIC_WRITE,
                                                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                          NULL,
                                                          OPEN_EXISTING,
                                                          FILE_ATTRIBUTE_NORMAL,
                                                          NULL);

    if (std::get<void *>(handle) == INVALID_HANDLE_VALUE) {
        if (SysCalls::getLastError() != ERROR_FILE_NOT_FOUND) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Open config file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
            return;
        }
        std::get<void *>(handle) = NEO::SysCalls::createFileA(configFilePath.c_str(),
                                                              GENERIC_READ | GENERIC_WRITE,
                                                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                              NULL,
                                                              CREATE_NEW,
                                                              FILE_ATTRIBUTE_NORMAL,
                                                              NULL);

        if (std::get<void *>(handle) == INVALID_HANDLE_VALUE) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Create config file failed! error code: %lu\n", GetCurrentProcessId(), GetLastError());
            std::get<void *>(handle) = NEO::SysCalls::createFileA(configFilePath.c_str(),
                                                                  GENERIC_READ | GENERIC_WRITE,
                                                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                                  NULL,
                                                                  OPEN_EXISTING,
                                                                  FILE_ATTRIBUTE_NORMAL,
                                                                  NULL);
        } else {
            countDirectorySize = true;
        }
    }

    OVERLAPPED overlapped = {0};
    auto result = NEO::SysCalls::lockFileEx(std::get<void *>(handle),
                                            LOCKFILE_EXCLUSIVE_LOCK,
                                            0,
                                            MAXDWORD,
                                            MAXDWORD,
                                            &overlapped);

    if (!result) {
        std::get<void *>(handle) = INVALID_HANDLE_VALUE;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Lock config file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return;
    }

    if (countDirectorySize) {
        const auto cacheFiles = getFiles(config.cacheDir);

        for (const auto &file : cacheFiles) {
            directorySize += static_cast<size_t>(file.fileSize);
        }
    } else {
        DWORD fPointer = NEO::SysCalls::setFilePointer(std::get<void *>(handle),
                                                       0,
                                                       NULL,
                                                       FILE_BEGIN);
        if (fPointer != 0) {
            directorySize = 0;
            unlockFileAndClose(std::get<void *>(handle));
            std::get<void *>(handle) = INVALID_HANDLE_VALUE;
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: File pointer move failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
            return;
        }

        DWORD numOfBytesRead = 0;
        result = NEO::SysCalls::readFile(std::get<void *>(handle),
                                         &directorySize,
                                         sizeof(directorySize),
                                         &numOfBytesRead,
                                         NULL);

        if (!result) {
            directorySize = 0;
            unlockFileAndClose(std::get<void *>(handle));
            std::get<void *>(handle) = INVALID_HANDLE_VALUE;
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Read config failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        }
    }
}

bool CompilerCache::createUniqueTempFileAndWriteData(char *tmpFilePath, const char *pBinary, size_t binarySize) {
    auto result = NEO::SysCalls::getTempFileNameA(config.cacheDir.c_str(), "TMP", 0, tmpFilePath);

    if (result == 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file name failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return false;
    }

    auto hTempFile = NEO::SysCalls::createFileA((LPCSTR)tmpFilePath,   // file name
                                                GENERIC_WRITE,         // open for write
                                                0,                     // do not share
                                                NULL,                  // default security
                                                CREATE_ALWAYS,         // overwrite existing
                                                FILE_ATTRIBUTE_NORMAL, // normal file
                                                NULL);                 // no template

    if (hTempFile == INVALID_HANDLE_VALUE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return false;
    }

    DWORD dwBytesWritten = 0;
    auto result2 = NEO::SysCalls::writeFile(hTempFile,
                                            pBinary,
                                            (DWORD)binarySize,
                                            &dwBytesWritten,
                                            NULL);

    if (!result2) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    } else if (dwBytesWritten != (DWORD)binarySize) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! Incorrect number of bytes written: %lu vs %lu\n", NEO::SysCalls::getProcessId(), dwBytesWritten, (DWORD)binarySize);
    }

    NEO::SysCalls::closeHandle(hTempFile);
    return true;
}

bool CompilerCache::renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) {
    return NEO::SysCalls::moveFileExA(oldName.c_str(), kernelFileHash.c_str(), MOVEFILE_REPLACE_EXISTING);
}

class HandleGuard : NonCopyableAndNonMovableClass {
  public:
    HandleGuard() = delete;
    explicit HandleGuard(void *&h) : handle(h) {}
    ~HandleGuard() {
        if (handle) {
            unlockFileAndClose(handle);
        }
    }

  private:
    void *handle = nullptr;
};

static_assert(NonCopyableAndNonMovable<HandleGuard>);

void writeDirSizeToConfigFile(void *hConfigFile, size_t directorySize) {
    DWORD sizeWritten = 0;
    OVERLAPPED overlapped = {0};
    auto result = NEO::SysCalls::writeFile(hConfigFile,
                                           &directorySize,
                                           (DWORD)sizeof(directorySize),
                                           &sizeWritten,
                                           &overlapped);

    if (!result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to config file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    } else if (sizeWritten != (DWORD)sizeof(directorySize)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to config file failed! Incorrect number of bytes written: %lu vs %lu\n", NEO::SysCalls::getProcessId(), sizeWritten, (DWORD)sizeof(directorySize));
    }
}

bool CompilerCache::cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize) {
    if (pBinary == nullptr || binarySize == 0 || binarySize > config.cacheSize) {
        return false;
    }

    std::unique_lock<std::mutex> lock(cacheAccessMtx);

    constexpr std::string_view configFileName = "config.file";
    std::string configFilePath = joinPath(config.cacheDir, configFileName.data());
    std::string cacheFilePath = joinPath(config.cacheDir, kernelFileHash + config.cacheFileExtension);

    UnifiedHandle hConfigFile{INVALID_HANDLE_VALUE};
    size_t directorySize = 0u;

    lockConfigFileAndReadSize(configFilePath, hConfigFile, directorySize);

    if (std::get<void *>(hConfigFile) == INVALID_HANDLE_VALUE) {
        return false;
    }

    HandleGuard configGuard(std::get<void *>(hConfigFile));

    DWORD cacheFileAttr = 0;
    cacheFileAttr = NEO::SysCalls::getFileAttributesA(cacheFilePath.c_str());

    if ((cacheFileAttr != INVALID_FILE_ATTRIBUTES) &&
        (SysCalls::getLastError() != ERROR_FILE_NOT_FOUND)) {
        return true;
    }

    const size_t maxSize = config.cacheSize;
    if (maxSize < (directorySize + binarySize)) {
        uint64_t bytesEvicted{0u};
        const auto evictSuccess = evictCache(bytesEvicted);
        const auto availableSpace = maxSize - directorySize + bytesEvicted;

        directorySize = std::max(static_cast<size_t>(0), directorySize - static_cast<size_t>(bytesEvicted));

        if (!evictSuccess || binarySize > availableSpace) {
            if (bytesEvicted > 0) {
                writeDirSizeToConfigFile(std::get<void *>(hConfigFile), directorySize);
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
        NEO::SysCalls::deleteFileA(tmpFilePath.c_str());
        return false;
    }

    directorySize += binarySize;
    writeDirSizeToConfigFile(std::get<void *>(hConfigFile), directorySize);

    return true;
}

std::unique_ptr<char[]> CompilerCache::loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize) {
    std::string filePath = joinPath(config.cacheDir, kernelFileHash + config.cacheFileExtension);
    return loadDataFromFile(filePath.c_str(), cachedBinarySize);
}
} // namespace NEO
