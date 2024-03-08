/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/stdio.h"

#include <cstring>
#include <new>
#include <set>

namespace NEO {
extern std::set<std::string> virtualFileList;
}

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize) {

    DEBUG_BREAK_IF(nullptr == pData);
    DEBUG_BREAK_IF(nullptr == filename);

    NEO::virtualFileList.insert(filename);

    return dataSize;
}

bool fileExists(const std::string &fileName) {
    FILE *pFile = nullptr;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

    if (NEO::virtualFileList.count(fileName) > 0) {
        return true;
    }

    fopen_s(&pFile, fileName.c_str(), "rb");
    if (pFile) {
        fclose(pFile);
    }
    return pFile != nullptr;
}

bool fileExistsHasSize(const std::string &fileName) {
    FILE *pFile = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

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
