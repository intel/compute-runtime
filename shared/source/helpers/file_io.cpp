/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "file_io.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/stdio.h"

#include <cstring>
#include <fstream>
#include <new>

size_t writeDataToFile(
    const char *filename,
    const void *pData,
    size_t dataSize) {
    FILE *fp = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(nullptr == pData);
    DEBUG_BREAK_IF(nullptr == filename);

    fopen_s(&fp, filename, "wb");
    if (fp) {
        nsize = fwrite(pData, sizeof(unsigned char), dataSize, fp);
        fclose(fp);
    }

    return nsize;
}

bool fileExists(const std::string &fileName) {
    FILE *pFile = nullptr;

    DEBUG_BREAK_IF(fileName.empty());
    DEBUG_BREAK_IF(fileName == "");

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

void dumpFileIncrement(const char *data, size_t dataSize, const std::string &filename, const std::string &extension) {
    std::ofstream fstream;
    auto filenameWithExt = filename + extension;
    int suffix = 0;
    while (fileExists(filenameWithExt)) {
        filenameWithExt = filename + "_" + std::to_string(suffix) + extension;
        suffix++;
    }
    writeDataToFile(filenameWithExt.c_str(), data, dataSize);
}
