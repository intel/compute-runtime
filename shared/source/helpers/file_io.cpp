/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "file_io.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/stdio.h"

#include <cstring>
#include <filesystem>
#include <mutex>
#include <new>

namespace NEO {

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

size_t writeDataToFile(
    const char *filename,
    std::string_view data,
    bool append) {
    FILE *fp = nullptr;
    size_t nsize = 0;

    DEBUG_BREAK_IF(nullptr == filename);
    static const std::string path = debugManager.flags.ForceLoggingDirectory.get() != "unk" ? debugManager.flags.ForceLoggingDirectory.get() : "";
    std::string fullPath = path + filename;
    static std::once_flag createDirectoryOnceFlag;
    std::call_once(createDirectoryOnceFlag, [&]() {
        if (!path.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
            UNRECOVERABLE_IF(ec.value() != 0);
        }
    });

    fopen_s(&fp, fullPath.c_str(), append ? "ab" : "wb");
    if (fp) {
        nsize = fwrite(data.data(), sizeof(unsigned char), data.size(), fp);
        fclose(fp);
    }

    return nsize;
}
} // namespace NEO
