/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/stdio.h"
#include "shared/source/utilities/io_functions.h"

#include <cstring>
#include <map>
#include <new>
#include <sstream>

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;
extern std::map<std::string, std::stringstream> virtualFileListTestKernelsOnly;
} // namespace NEO

size_t writeDataToFile(
    const char *filename,
    std::string_view data) {

    DEBUG_BREAK_IF(nullptr == filename);

    NEO::virtualFileList[filename] << data;

    return NEO::virtualFileList[filename].str().size();
}

bool fileExists(const std::string &fileName) {
    FILE *pFile = nullptr;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    if (NEO::virtualFileList.count(fileName) > 0) {
        return true;
    }

    pFile = NEO::IoFunctions::fopenPtr(fileName.c_str(), "rb");
    if (pFile) {
        NEO::IoFunctions::fclosePtr(pFile);
    }
    return pFile != nullptr;
}

bool fileExistsHasSize(const std::string &fileName) {
    FILE *pFile = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    auto it = NEO::virtualFileList.find(fileName);
    if (it != NEO::virtualFileList.end()) {
        std::stringstream &ss = it->second;
        ss.seekg(0, std::ios::end);
        return ss.tellg() > 0;
    }

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fseek(pFile, 0, SEEK_END);
        nsize = (size_t)ftell(pFile);
        fclose(pFile);
    }
    return pFile != nullptr && nsize > 0;
}

void removeVirtualFile(const std::string &fileName) {
    NEO::virtualFileList.erase(fileName);
}

bool virtualFileExists(const std::string &fileName) {
    if (NEO::virtualFileList.count(fileName) > 0) {
        return true;
    }
    return false;
}

std::unique_ptr<char[]> loadDataFromVirtualFile(
    const char *filename,
    size_t &retSize) {

    auto it = NEO::virtualFileList.find(filename);
    if (it == NEO::virtualFileList.end()) {
        retSize = 0;
        return nullptr;
    }
    std::unique_ptr<char[]> ret;

    std::stringstream &fileStream = it->second;

    fileStream.seekg(0, std::ios::end);
    retSize = static_cast<size_t>(fileStream.tellg());
    fileStream.seekg(0, std::ios::beg);

    ret.reset(new (std::nothrow) char[retSize + 1]);
    if (ret) {
        memset(ret.get(), 0x00, retSize + 1);
        fileStream.read(ret.get(), retSize);
    } else {
        retSize = 0;
    }

    return ret;
}

std::unique_ptr<char[]> loadDataFromVirtualFileTestKernelsOnly(const char *filename, size_t &retSize) {

    auto it = NEO::virtualFileListTestKernelsOnly.find(filename);
    if (it == NEO::virtualFileListTestKernelsOnly.end()) {
        retSize = 0;
        return nullptr;
    }
    std::stringstream &fileStream = it->second;
    fileStream.seekg(0, std::ios::end);
    retSize = static_cast<size_t>(fileStream.tellg());
    fileStream.seekg(0, std::ios::beg);

    auto ret = std::make_unique<char[]>(retSize + 1);
    if (ret) {
        ret[retSize] = 0x00;
        fileStream.read(ret.get(), retSize);
    } else {
        retSize = 0;
    }

    return ret;
}
