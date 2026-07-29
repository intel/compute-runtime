/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace L0 {
namespace Sysman {

static const std::vector<std::string> tracefsPaths = {"/sys/kernel/tracing", "/sys/kernel/debug/tracing"};
static const std::string xeErrorCperTracepointPath = "events/xe/xe_error_cper";

static bool hexStringToBytes(const std::string &hexStr, std::vector<uint8_t> &bytes) {
    bytes.clear();

    // Remove spaces from hex string (tracepoint data uses space-separated hex bytes)
    std::string cleanHex;
    cleanHex.reserve(hexStr.length());
    for (char c : hexStr) {
        if (c != ' ') {
            cleanHex += c;
        }
    }

    if (cleanHex.length() % 2 != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Invalid hex string length: %zu\n", __FUNCTION__, cleanHex.length());
        return false;
    }

    bytes.reserve(cleanHex.length() / 2);

    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteStr = cleanHex.substr(i, 2);
        char *end = nullptr;
        uint8_t byte = static_cast<uint8_t>(std::strtoul(byteStr.c_str(), &end, 16));
        if (end != byteStr.c_str() + 2) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): Invalid hex character at offset: %zu\n", __FUNCTION__, i);
            bytes.clear();
            return false;
        }
        bytes.push_back(byte);
    }
    return true;
}

// 'line' is a single tracefs event line, e.g.:
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05"
// 'fieldName' is the name of the field to extract (e.g., "cper_len" or "cper_raw").
static std::string extractFieldValue(const std::string &line, const std::string &fieldName) {
    std::string searchStr = fieldName + "=";
    size_t pos = line.find(searchStr);
    if (pos == std::string::npos) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Field '%s' not found in trace line\n", __FUNCTION__, fieldName.c_str());
        return "";
    }

    pos += searchStr.length();

    // Find the next field boundary: look for " <fieldname>=" pattern
    // This handles fields whose values contain spaces (e.g., hex byte sequences)
    size_t endPos = std::string::npos;
    size_t searchStart = pos;
    while (searchStart < line.length()) {
        size_t spacePos = line.find(' ', searchStart);
        if (spacePos == std::string::npos) {
            break;
        }
        // Check if the character after the space starts a new field (contains '=')
        size_t nextEquals = line.find('=', spacePos + 1);
        size_t nextSpace = line.find(' ', spacePos + 1);
        if (nextEquals != std::string::npos && (nextSpace == std::string::npos || nextEquals < nextSpace)) {
            endPos = spacePos;
            break;
        }
        searchStart = spacePos + 1;
    }

    if (endPos == std::string::npos) {
        endPos = line.length();
    }

    return line.substr(pos, endPos - pos);
}

std::unique_ptr<TraceFsApi> (*LinuxInfoLogImp::createTraceFsApi)() = []() {
    auto traceFsApi = std::make_unique<TraceFsApi>();
    traceFsApi->loadEntryPoints();
    return traceFsApi;
};

LinuxInfoLogImp::~LinuxInfoLogImp() = default;

std::vector<zes_intel_info_log_format_exp_t> OsInfoLog::getSupportedInfoLogFormats() {
    std::vector<zes_intel_info_log_format_exp_t> supportedFormats = {};
    for (const auto &tracingDir : tracefsPaths) {
        std::string tracepointPath = tracingDir + "/" + xeErrorCperTracepointPath + "/enable";
        int errorNum = 0;
        if (SysmanSysCallsWrapper::access(tracepointPath.c_str(), F_OK, errorNum) == 0) {
            supportedFormats.push_back(ZES_INTEL_INFO_LOG_FORMAT_CPER);
            break;
        }
    }

    return supportedFormats;
}

ze_result_t LinuxInfoLogImp::getProperties(zes_intel_info_log_properties_exp_t *pProperties) {

    pProperties->infoLogType = ZES_INTEL_INFO_LOG_TYPE_EXP_DEVICE;
    pProperties->infoLogFormat = infoLogFormat;
    long long bufferSize = pTraceFsApi->traceFsInstanceGetBufferSize(nullptr, -1);
    pProperties->maxSize = (bufferSize > 0) ? static_cast<uint32_t>(bufferSize) : 0u;

    return ZE_RESULT_SUCCESS;
}

// traceOutput is the raw content of the tracefs 'trace' file, e.g.:
//   # tracer: nop
//   #
//   # entries-in-buffer/entries-written: 2/2   #P:8
//   #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
//   #              | |       |   ||||       |         |
//        kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05
//        kworker-42   [000] ....  1234.567891: xe_error_cper: cper_len=4 cper_raw=DE AD BE EF
uint32_t LinuxInfoLogImp::countCperRecords(const std::string &traceOutput, uint32_t bufferSize) {
    std::istringstream iss(traceOutput);
    std::string line;
    uint32_t cperCount = 0;
    uint32_t accumulatedSize = 0;

    while (std::getline(iss, line)) {
        if (line.find("xe_error_cper:") == std::string::npos) {
            continue;
        }

        std::string cperLenStr = extractFieldValue(line, "cper_len");

        uint32_t cperLen = static_cast<uint32_t>(std::strtoul(cperLenStr.c_str(), nullptr, 0));
        if (cperLen == 0) {
            continue;
        }

        if (cperLen > (bufferSize - accumulatedSize)) {
            break;
        }

        accumulatedSize += cperLen;
        cperCount++;
    }

    return cperCount;
}

// Reads exactly 'cperCount' CPER records from the tracefs 'trace_pipe' (consuming stream)
// and writes their raw binary payloads contiguously into 'pBuffer'.
// On return, 'aggregatedCperLen' holds the total number of bytes written.
//
// 'trace_pipe' is opened O_NONBLOCK; the loop reads one byte at a time, accumulating
// characters into 'lineBuffer' until a newline is encountered. Each completed line that
// contains "xe_error_cper:" is parsed for two fields:
//
//   cper_len  - decimal byte count of the payload, e.g. "8"
//   cper_raw  - space-separated hex bytes of the payload, e.g. "AB CD EF 01 02 03 04 05"
//
// Sample trace_pipe line (one record):
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05\n"
//
// After parsing that line the bytes {0xAB,0xCD,0xEF,0x01,0x02,0x03,0x04,0x05} are
// memcpy'd into pBuffer and aggregatedCperLen is advanced by 8.
ze_result_t LinuxInfoLogImp::extractCperRecords(uint32_t cperCount, uint8_t *pBuffer, uint32_t bufferSize, uint32_t &aggregatedCperLen) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    int errorNum = 0;
    int fd = -1;
    for (const auto &tracingDir : tracefsPaths) {
        std::string tracePipePath = tracingDir + "/trace_pipe";
        fd = SysmanSysCallsWrapper::open(tracePipePath.c_str(), O_RDONLY | O_NONBLOCK, errorNum);
        if (fd >= 0) {
            break;
        }
    }

    if (fd < 0) {
        result = LinuxSysmanImp::getResult(errorNum);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Failed to open trace_pipe, returning error: 0x%x\n", __FUNCTION__, result);
        return result;
    }

    aggregatedCperLen = 0;
    uint32_t cperExtracted = 0;
    std::string lineBuffer;
    char byte;

    while (cperExtracted < cperCount) {
        ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, &byte, 1, errorNum);

        if (bytesRead < 0) {
            if (errorNum != EAGAIN) {
                result = LinuxSysmanImp::getResult(errorNum);
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                             "Error@ %s(): Failed to read from trace_pipe, returning error: 0x%x\n",
                             __FUNCTION__, result);
            }
            break;
        }

        if (bytesRead == 0) {
            break;
        }

        if (byte != '\n') {
            lineBuffer += byte;
            continue;
        }

        if (lineBuffer.find("xe_error_cper:") == std::string::npos) {
            lineBuffer.clear();
            continue;
        }

        std::string cperLenStr = extractFieldValue(lineBuffer, "cper_len");

        uint32_t cperLen = static_cast<uint32_t>(std::strtoul(cperLenStr.c_str(), nullptr, 0));
        if (cperLen == 0) {
            lineBuffer.clear();
            continue;
        }

        std::string cperHexStr = extractFieldValue(lineBuffer, "cper_raw");
        if (cperHexStr.empty()) {
            lineBuffer.clear();
            continue;
        }

        std::vector<uint8_t> cperData;
        if (!hexStringToBytes(cperHexStr, cperData)) {
            lineBuffer.clear();
            continue;
        }

        if (cperData.size() != cperLen) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): CPER data size mismatch - expected: %u, got: %zu\n",
                         __FUNCTION__, cperLen, cperData.size());
            lineBuffer.clear();
            continue;
        }

        if (cperLen > (bufferSize - aggregatedCperLen)) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Info@ %s(): Buffer full, truncating at %u bytes \n",
                         __FUNCTION__, aggregatedCperLen);
            result = ZE_RESULT_WARNING_DROPPED_DATA;
            break;
        }

        std::memcpy(pBuffer + aggregatedCperLen, cperData.data(), cperData.size());
        aggregatedCperLen += cperLen;
        cperExtracted++;

        lineBuffer.clear();
    }

    SysmanSysCallsWrapper::close(fd, errorNum);

    return result;
}

ze_result_t LinuxInfoLogImp::infoLogRead(uint32_t *pSize, uint8_t *pBuffer) {
    if (pBuffer == nullptr || pSize == nullptr || *pSize == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Invalid argument - pBuffer: %p, pSize: %p, size: %u\n",
                     __FUNCTION__, pBuffer, static_cast<void *>(pSize), pSize ? *pSize : 0);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Phase 1: Read from 'trace' (non-consuming) to determine how many CPERs fit in pBuffer
    auto traceData = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(nullptr, "trace", nullptr), free);
    if (!traceData) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Failed to read trace file and returning error 0x%x\n", __FUNCTION__, ZE_RESULT_ERROR_UNKNOWN);
        *pSize = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string traceOutput(traceData.get());

    uint32_t cperCount = countCperRecords(traceOutput, *pSize);
    if (cperCount == 0) {
        *pSize = 0;
        return ZE_RESULT_SUCCESS;
    }

    // Phase 2: Read from 'trace_pipe' (consuming) exactly cperCount records
    uint32_t bytesExtracted = 0;
    ze_result_t result = extractCperRecords(cperCount, pBuffer, *pSize, bytesExtracted);
    if (result != ZE_RESULT_SUCCESS && result != ZE_RESULT_WARNING_DROPPED_DATA) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Failed to extract CPER records and returning error 0x%x\n", __FUNCTION__, result);
        *pSize = 0;
        return result;
    }
    *pSize = bytesExtracted;

    return result;
}

ze_result_t LinuxInfoLogImp::infoLogEnable(bool state) {

    ze_result_t errorResult = ZE_RESULT_SUCCESS;
    int result = 0;
    if (state) {
        result = pTraceFsApi->traceFsEventEnable(nullptr, "xe", "xe_error_cper");
        if (result != 0) {
            errorResult = ZE_RESULT_ERROR_UNKNOWN;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): Failed to enable xe_error_cper tracepoint event, result: 0x%x\n", __FUNCTION__, errorResult);
            return errorResult;
        }

        result = pTraceFsApi->traceFsTraceOn(nullptr);
        if (result != 0) {
            errorResult = ZE_RESULT_ERROR_UNKNOWN;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): Failed to turn tracing on, result: 0x%x\n", __FUNCTION__, errorResult);
            return errorResult;
        }
    } else {
        result = pTraceFsApi->traceFsEventDisable(nullptr, "xe", "xe_error_cper");
        if (result != 0) {
            errorResult = ZE_RESULT_ERROR_UNKNOWN;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): Failed to disable xe_error_cper tracepoint event, result: 0x%x\n", __FUNCTION__, errorResult);
            return errorResult;
        }

        result = pTraceFsApi->traceFsTraceOff(nullptr);
        if (result != 0) {
            errorResult = ZE_RESULT_ERROR_UNKNOWN;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Error@ %s(): Failed to turn tracing off, result: 0x%x\n", __FUNCTION__, errorResult);
            return errorResult;
        }
    }

    return errorResult;
}

} // namespace Sysman
} // namespace L0
