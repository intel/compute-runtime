/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
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
    std::string newPath;

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    if (path.c_str()[path.size() - 1] == '\\') {
        return files;
    } else {
        newPath = path + "/*";
    }

    hFind = NEO::SysCalls::findFirstFileA(newPath.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: File search failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return files;
    }

    do {
        auto fileName = joinPath(path, ffd.cFileName);
        if (fileName.find(".cl_cache") != fileName.npos ||
            fileName.find(".l0_cache") != fileName.npos ||
            fileName.find(".ocloc_cache") != fileName.npos) {
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
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Unlock file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    }

    NEO::SysCalls::closeHandle(std::get<void *>(handle));
}

bool CompilerCache::evictCache() {
    auto files = getFiles(config.cacheDir);
    auto evictionLimit = config.cacheSize / 3;
    uint64_t evictionSizeCount = 0;

    for (const auto &file : files) {
        NEO::SysCalls::deleteFileA(file.path.c_str());
        evictionSizeCount += file.fileSize;

        if (evictionSizeCount > evictionLimit) {
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
        std::get<void *>(handle) = NEO::SysCalls::createFileA(configFilePath.c_str(),
                                                              GENERIC_READ | GENERIC_WRITE,
                                                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                              NULL,
                                                              CREATE_NEW,
                                                              FILE_ATTRIBUTE_NORMAL,
                                                              NULL);

        if (std::get<void *>(handle) == INVALID_HANDLE_VALUE) {
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

    if (result == FALSE && SysCalls::getLastError() == ERROR_IO_PENDING) { // if file is already locked by somebody else
        DWORD numberOfBytesTransmitted = 0;
        result = NEO::SysCalls::getOverlappedResult(std::get<void *>(handle), &overlapped, &numberOfBytesTransmitted, TRUE);
    }

    if (!result) {
        std::get<void *>(handle) = INVALID_HANDLE_VALUE;
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Lock config file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return;
    }

    if (countDirectorySize) {
        auto files = getFiles(config.cacheDir);

        for (const auto &file : files) {
            directorySize += static_cast<size_t>(file.fileSize);
        }
    } else {
        memset(&overlapped, 0, sizeof(overlapped));
        result = NEO::SysCalls::readFileEx(std::get<void *>(handle), &directorySize, sizeof(directorySize), &overlapped, NULL);

        if (!result) {
            directorySize = 0;
            unlockFileAndClose(std::get<void *>(handle));
            std::get<void *>(handle) = INVALID_HANDLE_VALUE;
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Read config failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        }
    }
}

bool CompilerCache::createUniqueTempFileAndWriteData(char *tmpFilePath, const char *pBinary, size_t binarySize) {
    auto result = NEO::SysCalls::getTempFileNameA(config.cacheDir.c_str(), "TMP", 0, tmpFilePath);

    if (result == 0) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file name failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
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
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Creating temporary file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
        return false;
    }

    DWORD dwBytesWritten = 0;
    auto result2 = NEO::SysCalls::writeFile(hTempFile,
                                            pBinary,
                                            (DWORD)binarySize,
                                            &dwBytesWritten,
                                            NULL);

    if (!result2) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    } else if (dwBytesWritten != (DWORD)binarySize) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to temporary file failed! Incorrect number of bytes written: %lu vs %lu\n", NEO::SysCalls::getProcessId(), dwBytesWritten, (DWORD)binarySize);
    }

    NEO::SysCalls::closeHandle(hTempFile);
    return true;
}

bool CompilerCache::renameTempFileBinaryToProperName(const std::string &oldName, const std::string &kernelFileHash) {
    return NEO::SysCalls::moveFileExA(oldName.c_str(), kernelFileHash.c_str(), MOVEFILE_REPLACE_EXISTING);
}

class HandleGuard {
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

bool CompilerCache::cacheBinary(const std::string &kernelFileHash, const char *pBinary, size_t binarySize) {
    if (pBinary == nullptr || binarySize == 0) {
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
        if (SysCalls::getLastError() == ERROR_HANDLE_EOF) {
            NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache info]: deleting the corrupted config file\n", NEO::SysCalls::getProcessId());
            SysCalls::deleteFileA(configFilePath.c_str());
        }
        return false;
    }

    HandleGuard configGuard(std::get<void *>(hConfigFile));

    DWORD cacheFileAttr = 0;
    cacheFileAttr = NEO::SysCalls::getFileAttributesA(cacheFilePath.c_str());

    if ((cacheFileAttr != INVALID_FILE_ATTRIBUTES) &&
        (SysCalls::getLastError() != ERROR_FILE_NOT_FOUND)) {
        return true;
    }

    size_t maxSize = config.cacheSize;
    if (maxSize < (directorySize + binarySize)) {
        if (!evictCache()) {
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

    DWORD sizeWritten = 0;
    auto result = NEO::SysCalls::writeFile(std::get<void *>(hConfigFile),
                                           &directorySize,
                                           (DWORD)sizeof(directorySize),
                                           &sizeWritten,
                                           NULL);

    if (!result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to config file failed! error code: %lu\n", NEO::SysCalls::getProcessId(), SysCalls::getLastError());
    } else if (sizeWritten != (DWORD)sizeof(directorySize)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "PID %d [Cache failure]: Writing to config file failed! Incorrect number of bytes written: %lu vs %lu\n", NEO::SysCalls::getProcessId(), sizeWritten, (DWORD)sizeof(directorySize));
    }

    return true;
}

std::unique_ptr<char[]> CompilerCache::loadCachedBinary(const std::string &kernelFileHash, size_t &cachedBinarySize) {
    std::string filePath = joinPath(config.cacheDir, kernelFileHash + config.cacheFileExtension);
    return loadDataFromFile(filePath.c_str(), cachedBinarySize);
}
} // namespace NEO
