/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
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

    // Remove all whitespace from hex string (spaces, newlines, tabs, etc.)
    std::string cleanHex;
    cleanHex.reserve(hexStr.length());
    for (char c : hexStr) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleanHex += c;
        }
    }

    if (cleanHex.length() % 2 != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid hex string length: %zu\n", __FUNCTION__, cleanHex.length());
        return false;
    }

    bytes.reserve(cleanHex.length() / 2);

    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteStr = cleanHex.substr(i, 2);
        char *end = nullptr;
        uint8_t byte = static_cast<uint8_t>(std::strtoul(byteStr.c_str(), &end, 16));
        if (end != byteStr.c_str() + 2) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid hex character at offset: %zu\n", __FUNCTION__, i);
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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Field '%s' not found in trace line\n", __FUNCTION__, fieldName.c_str());
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

LinuxInfoLogImp::~LinuxInfoLogImp() {
    LinuxInfoLogImp::infoLogDisable();
}

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

    // Query buffer size based on current state:
    // - If enabled: query active instance (global or named)
    // - If not enabled: query default global buffer size
    struct tracefs_instance *queryInstance = isEnabled ? pTraceFsInstance : nullptr;
    long long bufferSize = pTraceFsApi->traceFsInstanceGetBufferSize(queryInstance, -1);
    pProperties->maxSize = (bufferSize > 0) ? static_cast<uint32_t>(bufferSize) : 0u;
    pProperties->isInstancedCollectionSupported = isInstancedCollectionAvailable();

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

int LinuxInfoLogImp::getTracePipeFd() const {
    if (globalSysmanDriver == nullptr) {
        return -1;
    }
    auto pLinuxSysmanDriverImp = static_cast<LinuxSysmanDriverImp *>(globalSysmanDriver->pOsSysmanDriver);
    return (pLinuxSysmanDriverImp != nullptr) ? pLinuxSysmanDriverImp->getCperTracePipeFd() : -1;
}

// Opens the consuming 'trace_pipe' stream of 'instance' ('nullptr' selects the global
// tracefs buffer) and hands the descriptor over to the sysman driver, which owns it so
// that the events path can poll it for CPER notifications.
ze_result_t LinuxInfoLogImp::openTracePipe(struct tracefs_instance *instance) {
    auto pLinuxSysmanDriverImp = (globalSysmanDriver != nullptr) ? static_cast<LinuxSysmanDriverImp *>(globalSysmanDriver->pOsSysmanDriver) : nullptr;
    if (pLinuxSysmanDriverImp == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Os Sysman driver not initialized, returning error: 0x%x\n", __FUNCTION__, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (pLinuxSysmanDriverImp->getCperTracePipeFd() >= 0) {
        return ZE_RESULT_SUCCESS;
    }

    int errorNum = 0;
    int fd = -1;
    if (instance != nullptr) {
        char *tracePipePath = pTraceFsApi->traceFsInstanceGetFile(instance, "trace_pipe");
        if (tracePipePath == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get trace_pipe path for instance '%s'\n",
                         __FUNCTION__, activeInstanceName.c_str());
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        fd = SysmanSysCallsWrapper::open(tracePipePath, O_RDONLY | O_NONBLOCK, errorNum);
        pTraceFsApi->traceFsPutTracingFile(tracePipePath);
    } else {
        for (const auto &tracingDir : tracefsPaths) {
            std::string tracePipePath = tracingDir + "/trace_pipe";
            fd = SysmanSysCallsWrapper::open(tracePipePath.c_str(), O_RDONLY | O_NONBLOCK, errorNum);
            if (fd >= 0) {
                break;
            }
        }
    }

    if (fd < 0) {
        ze_result_t result = LinuxSysmanImp::getResult(errorNum);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Failed to open trace_pipe, returning error: 0x%x\n", __FUNCTION__, result);
        return result;
    }

    lineBuffer.clear();
    pLinuxSysmanDriverImp->setCperTracePipeFd(fd);
    return ZE_RESULT_SUCCESS;
}

// Lowers the tracefs wake watermark of 'instance' so that a single buffered event wakes a
// poll() waiter immediately instead of waiting for the buffer to reach its default fill
// level. Only used when the caller did not request an explicit threshold. Best effort:
// collection still works, just with delayed notifications, when the watermark cannot be
// programmed.
void LinuxInfoLogImp::setImmediateWakeBufferPercent(struct tracefs_instance *instance) {
    constexpr int immediateWakeBufferPercent = 0;
    int currentPercent = pTraceFsApi->traceFsInstanceGetBufferPercent(instance);
    if (currentPercent < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to read tracefs buffer percent, event reporting may be delayed\n", __FUNCTION__);
        return;
    }

    if (currentPercent == immediateWakeBufferPercent) {
        return;
    }

    if (pTraceFsApi->traceFsInstanceSetBufferPercent(instance, immediateWakeBufferPercent) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to set tracefs buffer percent to %d, event reporting may be delayed\n",
                     __FUNCTION__, immediateWakeBufferPercent);
        return;
    }

    savedBufferPercent = currentPercent;
    savedBufferPercentInstance = instance;
}

void LinuxInfoLogImp::restoreBufferPercent() {
    if (savedBufferPercent < 0) {
        return;
    }

    if (pTraceFsApi->traceFsInstanceSetBufferPercent(savedBufferPercentInstance, savedBufferPercent) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to restore tracefs buffer percent to %d, the watermark stays overridden until a later attempt succeeds\n",
                     __FUNCTION__, savedBufferPercent);
        return;
    }

    savedBufferPercent = -1;
    savedBufferPercentInstance = nullptr;
}

void LinuxInfoLogImp::closeTracePipe() {
    auto pLinuxSysmanDriverImp = (globalSysmanDriver != nullptr) ? static_cast<LinuxSysmanDriverImp *>(globalSysmanDriver->pOsSysmanDriver) : nullptr;
    if (pLinuxSysmanDriverImp == nullptr) {
        return;
    }

    int fd = pLinuxSysmanDriverImp->getCperTracePipeFd();
    if (fd < 0) {
        return;
    }

    int errorNum = 0;
    SysmanSysCallsWrapper::close(fd, errorNum);
    pLinuxSysmanDriverImp->setCperTracePipeFd(-1);
    lineBuffer.clear();
}

// Reads exactly 'cperCount' CPER records from the tracefs 'trace_pipe' (consuming stream)
// and writes their raw binary payloads contiguously into 'pBuffer'.
// On return, 'aggregatedCperLen' holds the total number of bytes written.
//
// 'trace_pipe' is opened O_NONBLOCK; the loop reads one byte at a time, accumulating
// characters into the 'lineBuffer' member until a newline is encountered. 'lineBuffer'
// deliberately survives across calls so that a line split by a short read (EAGAIN) is
// completed by the next call instead of being lost. Each completed line that contains
// "xe_error_cper:" is parsed for two fields:
//
//   cper_len  - decimal byte count of the payload, e.g. "8"
//   cper_raw  - space-separated hex bytes of the payload, e.g. "AB CD EF 01 02 03 04 05"
//
// Sample trace_pipe line (one record):
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05\n"
//
// After parsing that line the bytes {0xAB,0xCD,0xEF,0x01,0x02,0x03,0x04,0x05} are
// memcpy'd into pBuffer and aggregatedCperLen is advanced by 8.
ze_result_t LinuxInfoLogImp::extractCperRecords(int fd, uint32_t cperCount, uint8_t *pBuffer, uint32_t bufferSize, uint32_t &aggregatedCperLen) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    int errorNum = 0;

    aggregatedCperLen = 0;
    uint32_t cperExtracted = 0;
    char byte;

    while (cperExtracted < cperCount) {
        ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, &byte, 1, errorNum);

        if (bytesRead < 0) {
            if (errorNum != EAGAIN) {
                // The stream is broken, so the partially accumulated line can never be completed.
                lineBuffer.clear();
                result = LinuxSysmanImp::getResult(errorNum);
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read from trace_pipe, returning error: 0x%x\n",
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

        std::string line;
        line.swap(lineBuffer);

        if (line.find("xe_error_cper:") == std::string::npos) {
            continue;
        }

        std::string cperLenStr = extractFieldValue(line, "cper_len");

        uint32_t cperLen = static_cast<uint32_t>(std::strtoul(cperLenStr.c_str(), nullptr, 0));
        if (cperLen == 0) {
            continue;
        }

        std::string cperHexStr = extractFieldValue(line, "cper_raw");
        if (cperHexStr.empty()) {
            continue;
        }

        std::vector<uint8_t> cperData;
        if (!hexStringToBytes(cperHexStr, cperData)) {
            continue;
        }

        if (cperData.size() != cperLen) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): CPER data size mismatch - expected: %u, got: %zu\n",
                         __FUNCTION__, cperLen, cperData.size());
            continue;
        }

        if (cperLen > (bufferSize - aggregatedCperLen)) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Info@ %s(): Buffer full, truncating at %u bytes \n",
                         __FUNCTION__, aggregatedCperLen);
            result = ZE_RESULT_WARNING_DROPPED_DATA;
            break;
        }

        std::memcpy(pBuffer + aggregatedCperLen, cperData.data(), cperData.size());
        aggregatedCperLen += cperLen;
        cperExtracted++;
    }

    return result;
}

ze_result_t LinuxInfoLogImp::infoLogRead(uint32_t *pSize, uint8_t *pBuffer) {
    if (pBuffer == nullptr || pSize == nullptr || *pSize == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid argument - pBuffer: %p, pSize: %p, size: %u\n",
                     __FUNCTION__, pBuffer, static_cast<void *>(pSize), pSize ? *pSize : 0);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    int fd = getTracePipeFd();
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Info log collection is not enabled, returning error: 0x%x\n",
                     __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        *pSize = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Phase 1: Read from 'trace' (non-consuming) to determine how many CPERs fit in pBuffer
    // Read from the active instance when enabled with a named instance; pTraceFsInstance is
    // nullptr for global tracefs collection, which libtracefs maps to the global buffer.
    auto traceData = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(pTraceFsInstance, "trace", nullptr), free);
    if (!traceData) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read trace file and returning error 0x%x\n", __FUNCTION__, ZE_RESULT_ERROR_UNKNOWN);
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
    ze_result_t result = extractCperRecords(fd, cperCount, pBuffer, *pSize, bytesExtracted);
    if (result != ZE_RESULT_SUCCESS && result != ZE_RESULT_WARNING_DROPPED_DATA) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to extract CPER records and returning error 0x%x\n", __FUNCTION__, result);
        *pSize = 0;
        return result;
    }
    *pSize = bytesExtracted;

    return result;
}

bool LinuxInfoLogImp::isInstancedCollectionAvailable() {
    int errorNum = 0;
    for (const auto &tracingDir : tracefsPaths) {
        std::string instancesDir = tracingDir + "/instances";
        if (SysmanSysCallsWrapper::access(instancesDir.c_str(), W_OK, errorNum) == 0) {
            return true;
        }
    }
    return false;
}

bool LinuxInfoLogImp::checkInstancePreExists(const char *instanceName) {
    int errorNum = 0;
    for (const auto &tracingDir : tracefsPaths) {
        std::string instanceDir = tracingDir + "/instances/" + instanceName;
        if (SysmanSysCallsWrapper::access(instanceDir.c_str(), F_OK, errorNum) == 0) {
            return true;
        }
    }
    return false;
}

bool LinuxInfoLogImp::checkEventEnabled(struct tracefs_instance *instance) {
    auto data = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(instance, "events/xe/xe_error_cper/enable", nullptr), free);
    bool enabled = data && data.get()[0] == '1';
    return enabled;
}

bool LinuxInfoLogImp::checkTracingOn(struct tracefs_instance *instance) {
    auto data = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(instance, "tracing_on", nullptr), free);
    bool tracingOn = data && data.get()[0] == '1';
    return tracingOn;
}

void LinuxInfoLogImp::cleanupInstanceOnFailure(struct tracefs_instance *instance, bool isNew) {
    if (instance == nullptr) {
        return;
    }
    if (isNew) {
        pTraceFsApi->traceFsInstanceDestroy(instance);
    }
    pTraceFsApi->traceFsInstanceFree(instance);
}

ze_result_t LinuxInfoLogImp::infoLogEnable(zes_intel_info_log_enable_descriptor_exp *pEnableDescriptor) {
    const char *instanceName = pEnableDescriptor->instanceName;
    uint32_t *pBufferSizeInKb = pEnableDescriptor->pBufferSizeInKb;
    uint32_t *pPercentFullThreshold = pEnableDescriptor->pPercentFullThreshold;

    if (isEnabled) {
        bool requestingGlobal = (instanceName == nullptr);
        bool currentIsGlobal = (pTraceFsInstance == nullptr);

        if (requestingGlobal && currentIsGlobal) {
            return ZE_RESULT_SUCCESS;
        }
        if (!requestingGlobal && !currentIsGlobal && activeInstanceName == instanceName) {
            return ZE_RESULT_SUCCESS;
        }
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Already enabled with conflicting instance configuration\n", __FUNCTION__);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    struct tracefs_instance *instance = nullptr;
    bool isNewInstance = false;

    if (instanceName != nullptr) {
        if (!isInstancedCollectionAvailable()) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Named tracefs instances not available\n", __FUNCTION__);
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        bool preExisting = checkInstancePreExists(instanceName);

        instance = pTraceFsApi->traceFsInstanceCreate(instanceName);
        if (instance == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to create tracefs instance '%s'\n", __FUNCTION__, instanceName);
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        instanceWasPreExisting = preExisting;
        isNewInstance = !preExisting;
    }

    // Set buffer size if requested. libtracefs takes the size in kilobytes and writes it verbatim
    // to 'buffer_size_kb', so the caller's KB value is passed through unscaled.
    if (pBufferSizeInKb != nullptr && *pBufferSizeInKb > 0) {
        int result = pTraceFsApi->traceFsInstanceSetBufferSize(instance, static_cast<size_t>(*pBufferSizeInKb), -1); // -1 = all CPUs
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set buffer size to %u KB\n", __FUNCTION__, *pBufferSizeInKb);
            cleanupInstanceOnFailure(instance, isNewInstance);
            instanceWasPreExisting = false;
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        // Read back the size actually applied, which the kernel rounds up to its sub-buffer
        // granularity. A single CPU is queried on purpose: setting with cpu -1 programs the per-CPU
        // buffer size, whereas getting with cpu -1 would report the sum across all CPUs.
        long long actualSizeKb = pTraceFsApi->traceFsInstanceGetBufferSize(instance, 0);
        *pBufferSizeInKb = (actualSizeKb > 0) ? static_cast<uint32_t>(actualSizeKb) : 0u;
    }

    // Set buffer percent threshold if requested
    if (pPercentFullThreshold != nullptr) {
        uint32_t requestedPercent = *pPercentFullThreshold;
        int result = pTraceFsApi->traceFsInstanceSetBufferPercent(instance, static_cast<int>(requestedPercent));
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set buffer percent threshold to %u%%\n",
                         __FUNCTION__, requestedPercent);
            cleanupInstanceOnFailure(instance, isNewInstance);
            instanceWasPreExisting = false;
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        // Read back actual percent set
        int actualPercent = pTraceFsApi->traceFsInstanceGetBufferPercent(instance);
        *pPercentFullThreshold = static_cast<uint32_t>(actualPercent);
    } else if (infoLogFormat == ZES_INTEL_INFO_LOG_FORMAT_CPER) {
        // The caller did not ask for a specific fill level, and CPER records are streamed out
        // of 'trace_pipe', so drop the wake watermark to zero to get notified as soon as a
        // single record lands. The previous value is remembered and written back on disable.
        setImmediateWakeBufferPercent(instance);
    }

    bool alreadyEnabled = checkEventEnabled(instance);
    bool alreadyOn = checkTracingOn(instance);

    if (!alreadyEnabled) {
        int result = pTraceFsApi->traceFsEventEnable(instance, "xe", "xe_error_cper");
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to enable xe_error_cper tracepoint\n", __FUNCTION__);
            restoreBufferPercent();
            cleanupInstanceOnFailure(instance, isNewInstance);
            instanceWasPreExisting = false;
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    if (!alreadyOn) {
        int result = pTraceFsApi->traceFsTraceOn(instance);
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to turn tracing on\n", __FUNCTION__);
            if (!alreadyEnabled) {
                pTraceFsApi->traceFsEventDisable(instance, "xe", "xe_error_cper");
            }
            restoreBufferPercent();
            cleanupInstanceOnFailure(instance, isNewInstance);
            instanceWasPreExisting = false;
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    pTraceFsInstance = instance;
    isEnabled = true;
    eventWasAlreadyEnabled = alreadyEnabled;
    tracingWasAlreadyOn = alreadyOn;
    if (instanceName != nullptr) {
        activeInstanceName = instanceName;
    }

    // Only the CPER format streams records out of 'trace_pipe'; the other formats just need
    // the tracepoint enabled.
    if (infoLogFormat == ZES_INTEL_INFO_LOG_FORMAT_CPER) {
        ze_result_t result = openTracePipe(pTraceFsInstance);
        if (result != ZE_RESULT_SUCCESS) {
            // infoLogDisable() unwinds everything published above, including the watermark.
            infoLogDisable();
            return result;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxInfoLogImp::infoLogDisable() {
    if (!isEnabled) {
        // A previous disable may have failed to write the wake watermark back; retry it here.
        restoreBufferPercent();
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t status = ZE_RESULT_SUCCESS;

    closeTracePipe();
    restoreBufferPercent();

    if (!eventWasAlreadyEnabled) {
        int result = pTraceFsApi->traceFsEventDisable(pTraceFsInstance, "xe", "xe_error_cper");
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to disable xe_error_cper tracepoint\n", __FUNCTION__);
            status = ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    if (!tracingWasAlreadyOn) {
        int result = pTraceFsApi->traceFsTraceOff(pTraceFsInstance);
        if (result != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to turn tracing off\n", __FUNCTION__);
            status = ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    if (pTraceFsInstance != nullptr) {
        // The named instance is going away, and with it the buffer_percent file a failed
        // restore above would have retried against.
        savedBufferPercent = -1;
        savedBufferPercentInstance = nullptr;

        if (!instanceWasPreExisting) {
            pTraceFsApi->traceFsInstanceDestroy(pTraceFsInstance);
        }
        pTraceFsApi->traceFsInstanceFree(pTraceFsInstance);
    }

    pTraceFsInstance = nullptr;
    isEnabled = false;
    instanceWasPreExisting = false;
    eventWasAlreadyEnabled = false;
    tracingWasAlreadyOn = false;
    activeInstanceName.clear();

    return status;
}

// Parse BDF string "DDDD:BB:DD.F" (hex fields) into zes_pci_address_t.
static zes_pci_address_t parseBdf(const std::string &devStr) {
    zes_pci_address_t addr = {};
    unsigned int domain = 0, bus = 0, device = 0, function = 0;
    if (sscanf(devStr.c_str(), "%x:%x:%x.%x", &domain, &bus, &device, &function) == 4) {
        addr.domain = domain;
        addr.bus = bus;
        addr.device = device;
        addr.function = function;
    }
    return addr;
}

// Parse UUID string "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" into zes_uuid_t (big-endian byte array).
static zes_uuid_t parseUuid(const std::string &uuidStr) {
    zes_uuid_t uuid = {};
    unsigned int b[16] = {};
    // NOLINTNEXTLINE(cert-err34-c)
    if (sscanf(uuidStr.c_str(),
               "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
               &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]) == 16) {
        for (int i = 0; i < 16; i++) {
            uuid.id[i] = static_cast<uint8_t>(b[i]);
        }
    }
    return uuid;
}

// Extract timestamp (microseconds since boot) from a tracefs event line.
// The line has the format:
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: ..."
// Returns seconds*1_000_000 + fractional microseconds, or 0 on parse failure.
static uint64_t parseTimestamp(const std::string &line) {
    size_t markerPos = line.find(": xe_error_cper:");
    if (markerPos == std::string::npos) {
        return 0;
    }
    size_t tsStart = line.rfind(' ', markerPos - 1);
    if (tsStart == std::string::npos) {
        return 0;
    }
    std::string tsStr = line.substr(tsStart + 1, markerPos - tsStart - 1);
    size_t dotPos = tsStr.find('.');
    if (dotPos == std::string::npos) {
        return 0;
    }
    char *end = nullptr;
    uint64_t seconds = std::strtoull(tsStr.c_str(), &end, 10);
    std::string fracStr = tsStr.substr(dotPos + 1);
    while (fracStr.length() < 6) {
        fracStr += '0';
    }
    fracStr = fracStr.substr(0, 6);
    uint64_t microseconds = std::strtoull(fracStr.c_str(), &end, 10);
    uint64_t timestamp = seconds * 1000000ULL + microseconds;
    return timestamp;
}

void LinuxInfoLogImp::countCperRecordsAndSize(const std::string &traceOutput, uint32_t &cperCount, uint32_t &totalSize) {
    std::istringstream iss(traceOutput);
    std::string line;
    cperCount = 0;
    totalSize = 0;

    while (std::getline(iss, line)) {
        if (line.find("xe_error_cper:") == std::string::npos) {
            continue;
        }

        std::string cperLenStr = extractFieldValue(line, "cper_len");
        uint32_t cperLen = static_cast<uint32_t>(std::strtoul(cperLenStr.c_str(), nullptr, 0));
        if (cperLen == 0) {
            continue;
        }

        totalSize += cperLen;
        cperCount++;
    }
}

struct CperExtractionState {
    uint8_t *pBuffer;
    uint32_t bufferSize;
    zes_intel_info_log_metadata_exp *pDescriptors;
    uint32_t aggregatedCperLen = 0;
    uint32_t recordsExtracted = 0;
    uint32_t maxRecords;

    CperExtractionState(uint8_t *buffer, uint32_t bufSize,
                        zes_intel_info_log_metadata_exp *descriptors, uint32_t maxRecs)
        : pBuffer(buffer), bufferSize(bufSize), pDescriptors(descriptors),
          maxRecords(maxRecs) {}
};

// Process a single CPER line and extract data + metadata
// Returns true if a record was successfully processed and added to the buffer
// Sets bufferFull to true if buffer is full and processing should stop
static bool processCperLine(const std::string &line, CperExtractionState &state, bool &bufferFull) {
    bufferFull = false;

    // Check if this is a CPER event line
    if (line.find("xe_error_cper:") == std::string::npos) {
        return false;
    }

    // Extract CPER length
    std::string cperLenStr = extractFieldValue(line, "cper_len");
    uint32_t cperLen = static_cast<uint32_t>(std::strtoul(cperLenStr.c_str(), nullptr, 0));
    if (cperLen == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Skipping CPER record with zero or invalid cper_len field\n",
                     __FUNCTION__);
        return false;
    }

    // Extract CPER hex data
    std::string cperHexStr = extractFieldValue(line, "cper_raw");
    if (cperHexStr.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Skipping CPER record with missing or empty cper_raw field\n",
                     __FUNCTION__);
        return false;
    }

    // Convert hex string to bytes
    std::vector<uint8_t> cperData;
    if (!hexStringToBytes(cperHexStr, cperData)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Skipping CPER record due to invalid hex data in cper_raw field\n",
                     __FUNCTION__);
        return false;
    }

    // Validate data size matches expected length
    if (cperData.size() != cperLen) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): CPER data size mismatch - expected: %u, got: %zu\n",
                     __FUNCTION__, cperLen, cperData.size());
        return false;
    }

    // Check if record fits in buffer
    if (cperLen > (state.bufferSize - state.aggregatedCperLen)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Info@ %s(): Buffer full, truncating at %u bytes\n",
                     __FUNCTION__, state.aggregatedCperLen);
        bufferFull = true;
        return false;
    }

    // Fill per-record metadata
    zes_intel_info_log_metadata_exp &desc = state.pDescriptors[state.recordsExtracted];
    desc.lengthOfData = cperLen;
    desc.offset = state.aggregatedCperLen;
    desc.timestamp = parseTimestamp(line);
    desc.address = parseBdf(extractFieldValue(line, "dev"));
    desc.uuid = parseUuid(extractFieldValue(line, "platform_id"));

    // Copy CPER data to buffer
    std::memcpy(state.pBuffer + state.aggregatedCperLen, cperData.data(), cperData.size());
    state.aggregatedCperLen += cperLen;
    state.recordsExtracted++;

    return true;
}

// Helper function to build diagnostic context string for error messages
static std::string getTraceContext(struct tracefs_instance *instance, const std::string &instanceName) {
    if (instance == nullptr) {
        return "global tracefs";
    }
    return "instance '" + instanceName + "'";
}

ze_result_t LinuxInfoLogImp::infoLogReadWithMetaData(uint32_t *pSize, uint8_t *pBuffer,
                                                     uint32_t *pEventCount, zes_intel_info_log_metadata_exp *pDescriptors) {
    if (pSize == nullptr || pEventCount == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid argument - pSize: %p, pEventCount: %p\n",
                     __FUNCTION__, static_cast<void *>(pSize), static_cast<void *>(pEventCount));
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // Query mode: read non-consuming trace snapshot to determine size/count
    if (pBuffer == nullptr || pDescriptors == nullptr) {
        auto traceData = std::unique_ptr<char, decltype(&free)>(
            pTraceFsApi->traceFsInstanceFileRead(pTraceFsInstance, "trace", nullptr), free);
        if (!traceData) {
            std::string context = getTraceContext(pTraceFsInstance, activeInstanceName);
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read trace file from %s\n",
                         __FUNCTION__, context.c_str());
            *pSize = 0;
            *pEventCount = 0;
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        std::string traceOutput(traceData.get());
        uint32_t cperCount = 0;
        uint32_t totalSize = 0;
        countCperRecordsAndSize(traceOutput, cperCount, totalSize);
        *pEventCount = cperCount;
        *pSize = totalSize;
        return ZE_RESULT_SUCCESS;
    }

    // Extract mode: use trace_pipe for consuming read
    int tracePipeFd = getTracePipeFd();
    if (tracePipeFd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): trace_pipe not opened - call infoLogEnable first\n", __FUNCTION__);
        *pSize = 0;
        *pEventCount = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Read from trace_pipe (consuming) line-by-line and process each record immediately
    // Create fresh FILE* for this read session (dup() so we have independent fd)
    int errorNum = 0;
    int dupedFd = SysmanSysCallsWrapper::dup(tracePipeFd, errorNum);
    if (dupedFd < 0) {
        std::string context = getTraceContext(pTraceFsInstance, activeInstanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to dup trace_pipe fd %d for %s, errno=%d\n",
                     __FUNCTION__, tracePipeFd, context.c_str(), errorNum);
        *pSize = 0;
        *pEventCount = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    FILE *pTracePipeFile = SysmanSysCallsWrapper::fdopen(dupedFd, "r", errorNum);
    if (pTracePipeFile == nullptr) {
        std::string context = getTraceContext(pTraceFsInstance, activeInstanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to fdopen trace_pipe for %s, dup'd fd=%d, errno=%d\n",
                     __FUNCTION__, context.c_str(), dupedFd, errorNum);
        // Clean up the dup'd file descriptor to prevent leak
        SysmanSysCallsWrapper::close(dupedFd, errorNum);
        *pSize = 0;
        *pEventCount = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Disable FILE* buffering to prevent read-ahead data loss
    // Without this, fgets() may consume more data from trace_pipe than we process,
    // causing events to be lost when we exit the loop early (e.g., buffer full)
    if (SysmanSysCallsWrapper::setvbuf(pTracePipeFile, nullptr, _IONBF, 0) != 0) {
        SysmanSysCallsWrapper::fclose(pTracePipeFile, errorNum);
        *pSize = 0;
        *pEventCount = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Initialize state tracker for inline processing
    CperExtractionState state(pBuffer, *pSize, pDescriptors, *pEventCount);
    ze_result_t result = ZE_RESULT_SUCCESS;

    char readBuffer[kTraceLineBufferSize];
    std::string accumulatedLine; // For lines spanning multiple fgets() calls
    accumulatedLine.swap(lineBuffer);
    bool partialLineMayContinue = false;

    // Read and process lines until buffer full or requested count reached
    // Handle lines that may exceed buffer size by accumulating partial reads
    while (state.recordsExtracted < state.maxRecords) {
        if (SysmanSysCallsWrapper::fgets(readBuffer, sizeof(readBuffer), pTracePipeFile, errorNum) == nullptr) {
            if (errorNum == EAGAIN) {
                partialLineMayContinue = true;
            } else if (errorNum != 0) {
                accumulatedLine.clear();
                result = LinuxSysmanImp::getResult(errorNum);
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read from trace_pipe, returning error: 0x%x\n",
                             __FUNCTION__, result);
            }
            break;
        }

        size_t len = strlen(readBuffer);
        bool endsWithNewline = (len > 0 && readBuffer[len - 1] == '\n');

        accumulatedLine += readBuffer;

        // Safety check: detect corrupted data with excessively long lines
        if (accumulatedLine.size() > kMaxAccumulatedLineSize) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Line exceeds maximum size (%zu bytes), treating as corrupted - skipping to next newline\n",
                         __FUNCTION__, accumulatedLine.size());
            accumulatedLine.clear();
            // Skip to next newline to resynchronize
            while (SysmanSysCallsWrapper::fgets(readBuffer, sizeof(readBuffer), pTracePipeFile, errorNum) != nullptr) {
                size_t skipLen = strlen(readBuffer);
                if (skipLen > 0 && readBuffer[skipLen - 1] == '\n') {
                    break; // Found newline, resynchronized
                }
            }
            continue; // Skip to next iteration
        }

        if (endsWithNewline) {
            // Complete line accumulated - process it immediately
            bool bufferFull = false;
            processCperLine(accumulatedLine, state, bufferFull);

            if (bufferFull) {
                result = ZE_RESULT_WARNING_DROPPED_DATA;
                break;
            }

            accumulatedLine.clear(); // Reset for next line
        }
        // If no newline, partial line - keep accumulating
    }

    // Process any remaining incomplete line at the end
    if (!accumulatedLine.empty()) {
        if (partialLineMayContinue) {
            lineBuffer.swap(accumulatedLine);
        } else {
            bool bufferFull = false;
            processCperLine(accumulatedLine, state, bufferFull);
            if (bufferFull && result == ZE_RESULT_SUCCESS) {
                result = ZE_RESULT_WARNING_DROPPED_DATA;
            }
        }
    }

    // Close the FILE* (this also closes the dup'd fd)
    SysmanSysCallsWrapper::fclose(pTracePipeFile, errorNum);

    *pSize = state.aggregatedCperLen;
    *pEventCount = state.recordsExtracted;
    return result;
}

} // namespace Sysman
} // namespace L0
