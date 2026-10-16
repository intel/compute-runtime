/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/logger.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include <cstdio>
#include <string>

namespace NEO {

std::string getFileLoggerFileName(const DebugVariables &flags) {
    const auto &forcedLogFileName = flags.OverrideIgdrclLogFileName.get();
    if (forcedLogFileName != "unk") {
        return forcedLogFileName;
    }

    return "igdrcl_" + std::to_string(SysCalls::getProcessId()) + ".log";
}

FileLogger<globalDebugFunctionalityLevel> &fileLoggerInstance() {
    static FileLogger<globalDebugFunctionalityLevel> fileLoggerInstance(getFileLoggerFileName(debugManager.flags));
    return fileLoggerInstance;
}

UsmReusePerfLogger::UsmReusePerfLogger()
    : FileLogger(std::string("usm_reuse_perf.csv")) {
}

template <DebugFunctionalityLevel debugLevel>
FileLogger<debugLevel>::FileLogger(std::string filename) {
    this->logFileName = std::move(filename);
    if (enabled()) {
        std::remove(this->logFileName.c_str());
    }
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::writeToFile(std::string filename, const char *str, size_t length, std::ios_base::openmode mode) {
    std::lock_guard theLock(this->mutex);
    writeDataToFile(filename.c_str(), {str, length}, (mode & std::ios::app) != 0);
}

template <DebugFunctionalityLevel debugLevel>
void FileLogger<debugLevel>::logDebugString(bool enableLog, std::string_view debugString) {
    if (enabled()) {
        if (enableLog) {
            this->writeToFile(this->logFileName, debugString.data(), debugString.size(), std::ios::app);
        }
    }
}

template class FileLogger<DebugFunctionalityLevel::none>;
template class FileLogger<DebugFunctionalityLevel::regKeys>;
template class FileLogger<DebugFunctionalityLevel::full>;

template <>
void FileLoggerProxy<true>::logString(std::string_view data) {
    NEO::fileLoggerInstance().logDebugString(true, data);
}

} // namespace NEO
