/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_instance_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string_view>
#include <vector>

namespace L0 {
namespace Sysman {

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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid hex string length: %zu\n", NEO_FUNCTION_NAME, cleanHex.length());
        return false;
    }

    bytes.reserve(cleanHex.length() / 2);

    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteStr = cleanHex.substr(i, 2);
        char *end = nullptr;
        uint8_t byte = static_cast<uint8_t>(std::strtoul(byteStr.c_str(), &end, 16));
        if (end != byteStr.c_str() + 2) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid hex character at offset: %zu\n", NEO_FUNCTION_NAME, i);
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
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Field '%s' not found in trace line\n", NEO_FUNCTION_NAME, fieldName.c_str());
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

static const std::map<unsigned long, zes_intel_info_log_record_type_exp_t> cperSeverityToRecordType = {
    {0, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE},
    {1, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL},
    {2, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED},
    {3, ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL}};

static zes_intel_info_log_record_type_exp_t parseRecordType(const std::string &severityStr) {
    char *end = nullptr;
    unsigned long severity = std::strtoul(severityStr.c_str(), &end, 10);
    if (severityStr.empty() || end != severityStr.c_str() + severityStr.length()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Reporting unknown record type for unparsable severity: '%s'\n",
                     NEO_FUNCTION_NAME, severityStr.c_str());
        return ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN;
    }

    auto recordType = cperSeverityToRecordType.find(severity);
    if (recordType == cperSeverityToRecordType.end()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Reporting unknown record type for unrecognized severity: %lu\n",
                     NEO_FUNCTION_NAME, severity);
        return ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN;
    }
    return recordType->second;
}

// Extract timestamp (nanoseconds since boot) from a tracefs event line.
// The line has the format:
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: ..."
// The tracefs fractional part is microsecond grade, so the returned value is a whole number of
// microseconds. Returns 0 on parse failure.
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
    uint64_t timestamp = seconds * 1000000000ULL + microseconds * 1000ULL;
    return timestamp;
}

// traceOutput is the raw content of the tracefs 'trace' file, e.g.:
//   # tracer: nop
//   #
//   # entries-in-buffer/entries-written: 2/2   #P:8
//   #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
//   #              | |       |   ||||       |         |
//        kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05
//        kworker-42   [000] ....  1234.567891: xe_error_cper: cper_len=4 cper_raw=DE AD BE EF
static void countCperRecordsAndSize(const std::string &traceOutput, uint32_t &cperCount, uint32_t &totalSize) {
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
                     NEO_FUNCTION_NAME);
        return false;
    }

    // Extract CPER hex data
    std::string cperHexStr = extractFieldValue(line, "cper_raw");
    if (cperHexStr.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Skipping CPER record with missing or empty cper_raw field\n",
                     NEO_FUNCTION_NAME);
        return false;
    }

    // Convert hex string to bytes
    std::vector<uint8_t> cperData;
    if (!hexStringToBytes(cperHexStr, cperData)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Warning@ %s(): Skipping CPER record due to invalid hex data in cper_raw field\n",
                     NEO_FUNCTION_NAME);
        return false;
    }

    // Validate data size matches expected length
    if (cperData.size() != cperLen) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): CPER data size mismatch - expected: %u, got: %zu\n",
                     NEO_FUNCTION_NAME, cperLen, cperData.size());
        return false;
    }

    // Check if record fits in buffer
    if (cperLen > (state.bufferSize - state.aggregatedCperLen)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Info@ %s(): Buffer full, truncating at %u bytes\n",
                     NEO_FUNCTION_NAME, state.aggregatedCperLen);
        bufferFull = true;
        return false;
    }

    // Fill per-record metadata
    zes_intel_info_log_metadata_exp &desc = state.pDescriptors[state.recordsExtracted];
    desc.lengthOfData = cperLen;
    desc.offset = state.aggregatedCperLen;
    desc.timestamp = parseTimestamp(line);
    desc.address = parseBdf(extractFieldValue(line, "dev"));
    desc.uuid = parseUuid(extractFieldValue(line, "fru_id"));
    desc.recordType = parseRecordType(extractFieldValue(line, "severity"));

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

LinuxInfoLogInstanceImp::LinuxInfoLogInstanceImp(TraceFsApi *pTraceFsApi, zes_intel_info_log_format_exp_t format,
                                                 struct tracefs_instance *pTraceFsInstance, const std::string &instanceName,
                                                 bool instanceWasPreExisting, bool eventWasAlreadyEnabled, bool tracingWasAlreadyOn,
                                                 int ownershipFd)
    : pTraceFsApi(pTraceFsApi), infoLogFormat(format), pTraceFsInstance(pTraceFsInstance), instanceName(instanceName),
      instanceWasPreExisting(instanceWasPreExisting), eventWasAlreadyEnabled(eventWasAlreadyEnabled),
      tracingWasAlreadyOn(tracingWasAlreadyOn), ownershipFd(ownershipFd) {}

// The tracefs state teardown() reverts is global, so it must not be left applied if an instance is
// ever dropped without an explicit teardown. The call is qualified because virtual dispatch is
// already restricted to this class once the destructor runs.
LinuxInfoLogInstanceImp::~LinuxInfoLogInstanceImp() {
    LinuxInfoLogInstanceImp::teardown();
}

uint64_t LinuxInfoLogInstanceImp::getCurrentTimeInMs() {
    return SteadyClock::now().time_since_epoch().count();
}

ze_result_t LinuxInfoLogInstanceImp::applyBufferConfiguration(zes_intel_info_log_instance_exp_desc_t *pDesc) {
    ze_result_t result = applyBufferSize(pDesc->pBufferSize);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // Records are read out as soon as they are collected, so the wake watermark is always dropped to
    // zero to get notified as soon as a single record lands rather than once the buffer reaches its
    // default fill level. The previous value is remembered and written back on teardown.
    setImmediateWakeBufferPercent();

    return ZE_RESULT_SUCCESS;
}

// Counts the 'cpuN' entries of a tracefs 'per_cpu' directory, which holds one such entry per
// per-CPU collection buffer of the owning tracefs instance.
static uint32_t countPerCpuEntries(const std::string &perCpuPath) {
    DIR *dir = NEO::SysCalls::opendir(perCpuPath.c_str());
    if (dir == nullptr) {
        return 0;
    }

    uint32_t count = 0;
    struct dirent *entry = nullptr;
    while ((entry = NEO::SysCalls::readdir(dir)) != nullptr) {
        std::string_view name(entry->d_name);
        constexpr std::string_view cpuPrefix = "cpu";
        if (name.compare(0, cpuPrefix.size(), cpuPrefix) != 0) {
            continue;
        }

        std::string_view index = name.substr(cpuPrefix.size());
        if (!index.empty() && std::all_of(index.begin(), index.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
            count++;
        }
    }

    NEO::SysCalls::closedir(dir);
    return count;
}

// Number of per-CPU collection buffers backing this instance (a null tracefs instance selects the
// global tracefs buffer). Returns 0 when the 'per_cpu' directory cannot be scanned. The count cannot
// change while the instance is alive, so a successful scan is remembered.
uint32_t LinuxInfoLogInstanceImp::getPerCpuBufferCount() {
    if (perCpuBufferCount != 0) {
        return perCpuBufferCount;
    }

    perCpuBufferCount = scanPerCpuBufferCount();
    return perCpuBufferCount;
}

uint32_t LinuxInfoLogInstanceImp::scanPerCpuBufferCount() {
    if (pTraceFsInstance != nullptr) {
        char *perCpuPath = pTraceFsApi->traceFsInstanceGetFile(pTraceFsInstance, "per_cpu");
        if (perCpuPath == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get per_cpu path for instance '%s'\n",
                         NEO_FUNCTION_NAME, instanceName.c_str());
            return 0;
        }

        uint32_t count = countPerCpuEntries(perCpuPath);
        pTraceFsApi->traceFsPutTracingFile(perCpuPath);
        return count;
    }

    for (const auto &tracingDir : tracefsPaths) {
        uint32_t count = countPerCpuEntries(tracingDir + "/per_cpu");
        if (count > 0) {
            return count;
        }
    }

    return 0;
}

// Sums the counters of a single 'per_cpu/cpuN/stats' blob which report records the kernel ring
// buffer lost because the collection buffer was full: 'overrun' counts the records overwritten when
// the writer wrapped over records nobody had read yet, 'dropped events' the records the kernel
// refused to write while overwriting was disabled. A buffer runs in only one of those two modes, so
// the counters are added. 'commit overrun' is left out on purpose: it counts records lost to nested
// writes, which is not a buffer overflow.
//
// 'isComplete' reports whether every counter of the blob was read: it is false when a counter is
// there but its value cannot be parsed, and when the blob carries none of the counters at all. A sum
// missing a counter is not a count of the records lost, so the caller reports it as unknown instead
// of as a smaller loss.
static uint64_t parseDroppedRecordsFromStats(const char *stats, bool &isComplete) {
    constexpr std::string_view droppedRecordFields[] = {"overrun", "dropped events"};

    uint64_t droppedRecords = 0;
    uint32_t parsedFieldCount = 0;
    isComplete = true;
    std::istringstream iss(stats);
    std::string line;
    while (std::getline(iss, line)) {
        size_t separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        std::string_view field(line.c_str(), separator);
        if (std::find(std::begin(droppedRecordFields), std::end(droppedRecordFields), field) == std::end(droppedRecordFields)) {
            continue;
        }

        const char *value = line.c_str() + separator + 1;
        char *end = nullptr;
        unsigned long long parsedCount = std::strtoull(value, &end, 10);
        if (end == value) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Unparsable per-CPU stats line '%s'\n", NEO_FUNCTION_NAME, line.c_str());
            isComplete = false;
            continue;
        }

        droppedRecords += parsedCount;
        parsedFieldCount++;
    }

    if (parsedFieldCount == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Per-CPU stats report none of the dropped record counters\n", NEO_FUNCTION_NAME);
        isComplete = false;
    }

    return droppedRecords;
}

// Total number of records the kernel has lost across the per-CPU collection buffers of this
// instance. The counters are cumulative since the buffers were allocated, so callers turn them into
// a per-read delta. 'isComplete' reports whether every per-CPU buffer was accounted for: it is false
// when the buffers cannot be enumerated, and when a 'stats' file cannot be read or does not carry
// the counters, because the total would then be missing an unknown number of lost records.
uint64_t LinuxInfoLogInstanceImp::readDroppedRecordTotal(bool &isComplete) {
    isComplete = true;
    uint32_t bufferCount = getPerCpuBufferCount();
    if (bufferCount == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to determine the number of per-CPU collection buffers, dropped records cannot be counted\n", NEO_FUNCTION_NAME);
        isComplete = false;
        return 0;
    }

    uint64_t droppedRecords = 0;
    for (uint32_t cpu = 0; cpu < bufferCount; cpu++) {
        std::string statsFile = "per_cpu/cpu" + std::to_string(cpu) + "/stats";
        auto stats = std::unique_ptr<char, decltype(&free)>(
            pTraceFsApi->traceFsInstanceFileRead(pTraceFsInstance, statsFile.c_str(), nullptr), free);
        if (!stats) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Info@ %s(): Failed to read '%s' from %s\n",
                         NEO_FUNCTION_NAME, statsFile.c_str(), getTraceContext(pTraceFsInstance, instanceName).c_str());
            isComplete = false;
            continue;
        }

        bool statsAreComplete = true;
        droppedRecords += parseDroppedRecordsFromStats(stats.get(), statsAreComplete);
        isComplete = isComplete && statsAreComplete;
    }

    return droppedRecords;
}

// Records lost since the previous call, which is what a read or peek reports to the caller. The
// total read back is remembered so each loss is reported exactly once. Returns whether the count is
// the complete one for the interval, which is what the caller reports as isDroppedRecordCountValid.
bool LinuxInfoLogInstanceImp::consumeDroppedRecordCount(uint32_t &droppedRecordCount) {
    bool isComplete = true;
    uint64_t droppedRecordTotal = readDroppedRecordTotal(isComplete);

    // An incomplete total says nothing about how many records were lost, so no count is reported and
    // the baseline is left where it was. The loss is then reported in full by the first later call
    // which can read all of the counters, instead of being lost or counted twice.
    if (!isComplete) {
        droppedRecordCount = 0;
        return false;
    }

    // Resizing a buffer, or clearing it through tracefs, restarts the kernel counters. The baseline
    // then just follows them back down instead of underflowing into a huge count.
    uint64_t droppedRecords = (droppedRecordTotal > reportedDroppedRecordTotal) ? (droppedRecordTotal - reportedDroppedRecordTotal) : 0;
    reportedDroppedRecordTotal = droppedRecordTotal;

    droppedRecordCount = static_cast<uint32_t>(std::min<uint64_t>(droppedRecords, std::numeric_limits<uint32_t>::max()));
    return true;
}

// Total size, in kilobytes, of the collection buffer of this instance. Getting with cpu -1 reports
// the sum across its per-CPU buffers. A failed read reports 0 rather than the raw -1, which would
// reach the caller as 0xffffffff.
uint32_t LinuxInfoLogInstanceImp::readTotalBufferSize() {
    long long totalSizeKb = pTraceFsApi->traceFsInstanceGetBufferSize(pTraceFsInstance, -1); // -1 = all CPUs
    if (totalSizeKb <= 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Info@ %s(): Failed to read the buffer size of %s\n",
                     NEO_FUNCTION_NAME, getTraceContext(pTraceFsInstance, instanceName).c_str());
        return 0u;
    }

    return static_cast<uint32_t>(totalSizeKb);
}

// Resizes the collection buffer of this instance to the total size the caller asked for. libtracefs
// takes the size in kilobytes and programs it as the size of every per-CPU buffer, so the requested
// total is first split evenly across those buffers.
ze_result_t LinuxInfoLogInstanceImp::applyBufferSize(uint32_t *pBufferSize) {
    if (pBufferSize == nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    // A zero request keeps whatever size the buffer already has, and that size is what was applied,
    // so it is reported back. This lets a caller learn the size without changing it.
    if (*pBufferSize == 0) {
        *pBufferSize = readTotalBufferSize();
        return ZE_RESULT_SUCCESS;
    }

    uint32_t requestedSizeKb = *pBufferSize;
    uint32_t perCpuBufferCount = getPerCpuBufferCount();
    if (perCpuBufferCount == 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to determine the number of per-CPU collection buffers\n", NEO_FUNCTION_NAME);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Rounded up so the applied total is never smaller than the requested one.
    size_t perCpuSizeKb = (static_cast<size_t>(requestedSizeKb) + perCpuBufferCount - 1) / perCpuBufferCount;

    // A single CPU is queried on purpose: setting with cpu -1 programs the per-CPU buffer size,
    // which is what has to be written back on teardown.
    long long currentPerCpuSizeKb = pTraceFsApi->traceFsInstanceGetBufferSize(pTraceFsInstance, 0);

    if (pTraceFsApi->traceFsInstanceSetBufferSize(pTraceFsInstance, perCpuSizeKb, -1) != 0) { // -1 = all CPUs
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to set buffer size to %zu KB per CPU for a %u KB total\n",
                     NEO_FUNCTION_NAME, perCpuSizeKb, requestedSizeKb);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    savedPerCpuBufferSizeKb = currentPerCpuSizeKb;

    // Read back the total actually applied, which the kernel rounds up to its sub-buffer granularity.
    *pBufferSize = readTotalBufferSize();

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxInfoLogInstanceImp::startCollection() {
    // Records the buffers lost before this instance started collecting are not its to report, so the
    // kernel counters are baselined here rather than counted from zero. A total which could only be
    // read in part is still the best baseline available, and taking it only makes a later read report
    // a loss as newer than it was, rather than not report it at all.
    bool isBaselineComplete = true;
    reportedDroppedRecordTotal = readDroppedRecordTotal(isBaselineComplete);

    if (!eventWasAlreadyEnabled) {
        if (pTraceFsApi->traceFsEventEnable(pTraceFsInstance, "xe", "xe_error_cper") != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to enable xe_error_cper tracepoint\n", NEO_FUNCTION_NAME);
            restoreBufferConfiguration();
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    if (!tracingWasAlreadyOn) {
        if (pTraceFsApi->traceFsTraceOn(pTraceFsInstance) != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to turn tracing on\n", NEO_FUNCTION_NAME);
            if (!eventWasAlreadyEnabled) {
                pTraceFsApi->traceFsEventDisable(pTraceFsInstance, "xe", "xe_error_cper");
            }
            restoreBufferConfiguration();
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    // Only the CPER format streams records out of 'trace_pipe'; the other formats just need
    // the tracepoint enabled.
    if (infoLogFormat != ZES_INTEL_INFO_LOG_FORMAT_CPER) {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t result = openTracePipe();
    if (result != ZE_RESULT_SUCCESS) {
        if (!tracingWasAlreadyOn) {
            pTraceFsApi->traceFsTraceOff(pTraceFsInstance);
        }
        if (!eventWasAlreadyEnabled) {
            pTraceFsApi->traceFsEventDisable(pTraceFsInstance, "xe", "xe_error_cper");
        }
        restoreBufferConfiguration();
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxInfoLogInstanceImp::teardown() {
    if (tornDown) {
        return ZE_RESULT_SUCCESS;
    }
    tornDown = true;

    closeTracePipe();

    ze_result_t status = ZE_RESULT_SUCCESS;

    if (!eventWasAlreadyEnabled) {
        if (pTraceFsApi->traceFsEventDisable(pTraceFsInstance, "xe", "xe_error_cper") != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to disable xe_error_cper tracepoint\n", NEO_FUNCTION_NAME);
            status = ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    if (!tracingWasAlreadyOn) {
        if (pTraceFsApi->traceFsTraceOff(pTraceFsInstance) != 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to turn tracing off\n", NEO_FUNCTION_NAME);
            status = ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    restoreBufferConfiguration();

    if (pTraceFsInstance != nullptr) {
        if (!instanceWasPreExisting) {
            pTraceFsApi->traceFsInstanceDestroy(pTraceFsInstance);
        }
        pTraceFsApi->traceFsInstanceFree(pTraceFsInstance);
        pTraceFsInstance = nullptr;
    }

    // Released last, so that no other consumer can claim the name while this instance is still
    // reverting the collection state or destroying the tracefs instance behind it.
    if (ownershipFd >= 0) {
        int errorNum = 0;
        SysmanSysCallsWrapper::close(ownershipFd, errorNum);
        ownershipFd = -1;
    }

    lineBuffer.clear();

    return status;
}

// Lowers the tracefs wake watermark so that a single buffered event wakes a poll() waiter
// immediately instead of waiting for the buffer to reach its default fill level. Best effort:
// collection still works, just with delayed notifications, when the watermark cannot be programmed.
void LinuxInfoLogInstanceImp::setImmediateWakeBufferPercent() {
    constexpr int immediateWakeBufferPercent = 0;
    int currentPercent = pTraceFsApi->traceFsInstanceGetBufferPercent(pTraceFsInstance);
    if (currentPercent < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to read tracefs buffer percent, event reporting may be delayed\n", NEO_FUNCTION_NAME);
        return;
    }

    if (currentPercent == immediateWakeBufferPercent) {
        return;
    }

    if (pTraceFsApi->traceFsInstanceSetBufferPercent(pTraceFsInstance, immediateWakeBufferPercent) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to set tracefs buffer percent to %d, event reporting may be delayed\n",
                     NEO_FUNCTION_NAME, immediateWakeBufferPercent);
        return;
    }

    savedBufferPercent = currentPercent;
}

void LinuxInfoLogInstanceImp::restoreBufferPercent() {
    if (savedBufferPercent < 0) {
        return;
    }

    if (pTraceFsApi->traceFsInstanceSetBufferPercent(pTraceFsInstance, savedBufferPercent) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to restore tracefs buffer percent to %d, the watermark stays overridden until a later attempt succeeds\n",
                     NEO_FUNCTION_NAME, savedBufferPercent);
        return;
    }

    savedBufferPercent = -1;
}

void LinuxInfoLogInstanceImp::restoreBufferSize() {
    if (savedPerCpuBufferSizeKb < 0) {
        return;
    }

    if (pTraceFsApi->traceFsInstanceSetBufferSize(pTraceFsInstance, static_cast<size_t>(savedPerCpuBufferSizeKb), -1) != 0) { // -1 = all CPUs
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Info@ %s(): Failed to restore tracefs buffer size to %lld KB per CPU, the size stays overridden until a later attempt succeeds\n",
                     NEO_FUNCTION_NAME, savedPerCpuBufferSizeKb);
        return;
    }

    savedPerCpuBufferSizeKb = -1;
}

// Reverts everything applyBufferConfiguration() programmed on the shared tracefs buffer.
void LinuxInfoLogInstanceImp::restoreBufferConfiguration() {
    restoreBufferPercent();
    restoreBufferSize();
}

// Opens the consuming 'trace_pipe' stream of this collection instance (a null tracefs instance
// selects the global tracefs buffer) and registers the descriptor with the sysman driver so that
// the events path can poll it for CPER notifications.
ze_result_t LinuxInfoLogInstanceImp::openTracePipe() {
    auto pLinuxSysmanDriverImp = (globalSysmanDriver != nullptr) ? static_cast<LinuxSysmanDriverImp *>(globalSysmanDriver->pOsSysmanDriver) : nullptr;
    if (pLinuxSysmanDriverImp == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Error@ %s(): Os Sysman driver not initialized, returning error: 0x%x\n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    int errorNum = 0;
    int fd = -1;
    if (pTraceFsInstance != nullptr) {
        char *tracePipePath = pTraceFsApi->traceFsInstanceGetFile(pTraceFsInstance, "trace_pipe");
        if (tracePipePath == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get trace_pipe path for instance '%s'\n",
                         NEO_FUNCTION_NAME, instanceName.c_str());
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
                     "Error@ %s(): Failed to open trace_pipe, returning error: 0x%x\n", NEO_FUNCTION_NAME, result);
        return result;
    }

    lineBuffer.clear();
    tracePipeFd = fd;
    pLinuxSysmanDriverImp->registerCperTracePipeFd(tracePipeFd);
    return ZE_RESULT_SUCCESS;
}

void LinuxInfoLogInstanceImp::closeTracePipe() {
    if (tracePipeFd < 0) {
        return;
    }

    auto pLinuxSysmanDriverImp = (globalSysmanDriver != nullptr) ? static_cast<LinuxSysmanDriverImp *>(globalSysmanDriver->pOsSysmanDriver) : nullptr;
    if (pLinuxSysmanDriverImp != nullptr) {
        pLinuxSysmanDriverImp->unregisterCperTracePipeFd(tracePipeFd);
    }

    int errorNum = 0;
    SysmanSysCallsWrapper::close(tracePipeFd, errorNum);
    tracePipeFd = -1;
}

ze_result_t LinuxInfoLogInstanceImp::queryRecords(uint32_t *pSize, uint32_t *pRecordCount, bool &anyFound) {
    auto traceData = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(pTraceFsInstance, "trace", nullptr), free);
    if (!traceData) {
        std::string context = getTraceContext(pTraceFsInstance, instanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read trace file from %s\n",
                     NEO_FUNCTION_NAME, context.c_str());
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint32_t cperCount = 0;
    uint32_t totalSize = 0;
    countCperRecordsAndSize(traceData.get(), cperCount, totalSize);
    *pSize = totalSize;
    *pRecordCount = cperCount;
    anyFound = (cperCount != 0);

    return ZE_RESULT_SUCCESS;
}

// Reads CPER records from the consuming 'trace_pipe' stream, stopping at the caller's record count,
// at the caller's buffer size, at 'deadlineMs' or when the stream runs dry.
//
// The stream is opened O_NONBLOCK and read through an unbuffered FILE*, so that fgets() cannot read
// ahead past the records this call reports. Each completed line that contains "xe_error_cper:" is
// parsed for two fields:
//
//   cper_len  - decimal byte count of the payload, e.g. "8"
//   cper_raw  - space-separated hex bytes of the payload, e.g. "AB CD EF 01 02 03 04 05"
//
// Sample trace_pipe line (one record):
//   "     kworker-42   [000] ....  1234.567890: xe_error_cper: cper_len=8 cper_raw=AB CD EF 01 02 03 04 05\n"
//
// 'lineBuffer' deliberately survives across calls, so that a line split by a short read is
// completed by the next call instead of being lost.
ze_result_t LinuxInfoLogInstanceImp::extractFromTracePipe(uint64_t deadlineMs, uint32_t *pSize, uint8_t *pBuffer,
                                                          uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                          StopReason &stopReason) {
    int errorNum = 0;
    int dupedFd = SysmanSysCallsWrapper::dup(tracePipeFd, errorNum);
    if (dupedFd < 0) {
        std::string context = getTraceContext(pTraceFsInstance, instanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to dup trace_pipe fd %d for %s, errno=%d\n",
                     NEO_FUNCTION_NAME, tracePipeFd, context.c_str(), errorNum);
        *pSize = 0;
        *pRecordCount = 0;
        stopReason = StopReason::error;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    FILE *pTracePipeFile = SysmanSysCallsWrapper::fdopen(dupedFd, "r", errorNum);
    if (pTracePipeFile == nullptr) {
        std::string context = getTraceContext(pTraceFsInstance, instanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to fdopen trace_pipe for %s, dup'd fd=%d, errno=%d\n",
                     NEO_FUNCTION_NAME, context.c_str(), dupedFd, errorNum);
        SysmanSysCallsWrapper::close(dupedFd, errorNum);
        *pSize = 0;
        *pRecordCount = 0;
        stopReason = StopReason::error;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (SysmanSysCallsWrapper::setvbuf(pTracePipeFile, nullptr, _IONBF, 0) != 0) {
        SysmanSysCallsWrapper::fclose(pTracePipeFile, errorNum);
        *pSize = 0;
        *pRecordCount = 0;
        stopReason = StopReason::error;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    CperExtractionState state(pBuffer, *pSize, pDescriptors, *pRecordCount);
    ze_result_t result = ZE_RESULT_SUCCESS;
    stopReason = StopReason::recordLimit;

    char readBuffer[kTraceLineBufferSize];
    std::string accumulatedLine; // For lines spanning multiple fgets() calls
    accumulatedLine.swap(lineBuffer);
    bool partialLineMayContinue = false;
    // A complete line carried over from a previous call, held back because the buffer was full, has
    // to be processed before anything else is pulled from the stream. Otherwise the next fgets()
    // would be appended to it and the two records would be parsed as a single malformed line.
    bool endsWithNewline = !accumulatedLine.empty() && accumulatedLine.back() == '\n';

    while (state.recordsExtracted < state.maxRecords) {
        if (!endsWithNewline) {
            if (deadlineMs != UINT64_MAX && getCurrentTimeInMs() >= deadlineMs) {
                stopReason = StopReason::deadline;
                partialLineMayContinue = true;
                break;
            }

            if (SysmanSysCallsWrapper::fgets(readBuffer, sizeof(readBuffer), pTracePipeFile, errorNum) == nullptr) {
                if (errorNum == EAGAIN) {
                    stopReason = StopReason::drained;
                    partialLineMayContinue = true;
                } else if (errorNum != 0) {
                    accumulatedLine.clear();
                    stopReason = StopReason::error;
                    result = LinuxSysmanImp::getResult(errorNum);
                    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read from trace_pipe, returning error: 0x%x\n",
                                 NEO_FUNCTION_NAME, result);
                } else {
                    stopReason = StopReason::drained;
                }
                break;
            }

            size_t len = strlen(readBuffer);
            endsWithNewline = (len > 0 && readBuffer[len - 1] == '\n');

            accumulatedLine += readBuffer;

            // Safety check: detect corrupted data with excessively long lines
            if (accumulatedLine.size() > kMaxAccumulatedLineSize) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Line exceeds maximum size (%zu bytes), treating as corrupted - skipping to next newline\n",
                             NEO_FUNCTION_NAME, accumulatedLine.size());
                accumulatedLine.clear();
                // Skip to next newline to resynchronize
                while (SysmanSysCallsWrapper::fgets(readBuffer, sizeof(readBuffer), pTracePipeFile, errorNum) != nullptr) {
                    size_t skipLen = strlen(readBuffer);
                    if (skipLen > 0 && readBuffer[skipLen - 1] == '\n') {
                        break; // Found newline, resynchronized
                    }
                }
                endsWithNewline = false;
                continue;
            }
        }

        if (endsWithNewline) {
            bool bufferFull = false;
            processCperLine(accumulatedLine, state, bufferFull);

            if (bufferFull) {
                // The line has already been consumed from trace_pipe, so it is held back for the
                // next call instead of being lost. 'lineBuffer' was swapped empty above, so the
                // swap also leaves 'accumulatedLine' empty for the tail handling below.
                lineBuffer.swap(accumulatedLine);
                stopReason = StopReason::sizeLimit;
                break;
            }
            accumulatedLine.clear();
            endsWithNewline = false;
        }
        // If no newline, partial line - keep accumulating
    }

    if (!accumulatedLine.empty()) {
        bool bufferFull = false;
        if (!partialLineMayContinue) {
            processCperLine(accumulatedLine, state, bufferFull);
        }

        // Either the stream may still complete this line, or it holds a record which did not fit.
        // Both are carried over to the next call rather than dropped.
        if (partialLineMayContinue || bufferFull) {
            lineBuffer.swap(accumulatedLine);
            if (bufferFull) {
                stopReason = StopReason::sizeLimit;
            }
        }
    }

    SysmanSysCallsWrapper::fclose(pTracePipeFile, errorNum);

    *pSize = state.aggregatedCperLen;
    *pRecordCount = state.recordsExtracted;

    return result;
}

// Reads CPER records from the non-consuming 'trace' snapshot. Same stop rules as
// extractFromTracePipe, but the records stay available to later calls.
ze_result_t LinuxInfoLogInstanceImp::extractFromTraceSnapshot(uint64_t deadlineMs, uint32_t *pSize, uint8_t *pBuffer,
                                                              uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                              StopReason &stopReason) {
    auto traceData = std::unique_ptr<char, decltype(&free)>(
        pTraceFsApi->traceFsInstanceFileRead(pTraceFsInstance, "trace", nullptr), free);
    if (!traceData) {
        std::string context = getTraceContext(pTraceFsInstance, instanceName);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read trace file from %s\n",
                     NEO_FUNCTION_NAME, context.c_str());
        *pSize = 0;
        *pRecordCount = 0;
        stopReason = StopReason::error;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    CperExtractionState state(pBuffer, *pSize, pDescriptors, *pRecordCount);
    stopReason = StopReason::recordLimit;

    std::istringstream iss(traceData.get());
    std::string line;
    while (state.recordsExtracted < state.maxRecords) {
        if (deadlineMs != UINT64_MAX && getCurrentTimeInMs() >= deadlineMs) {
            stopReason = StopReason::deadline;
            break;
        }

        if (!std::getline(iss, line)) {
            stopReason = StopReason::drained;
            break;
        }

        bool bufferFull = false;
        processCperLine(line, state, bufferFull);
        if (bufferFull) {
            stopReason = StopReason::sizeLimit;
            break;
        }
    }

    *pSize = state.aggregatedCperLen;
    *pRecordCount = state.recordsExtracted;

    return ZE_RESULT_SUCCESS;
}

void LinuxInfoLogInstanceImp::fillReadStatus(zes_intel_info_log_read_status_exp_t *pReadStatus, StopReason stopReason,
                                             bool queryCall, bool anyRecordFound) {
    if (pReadStatus == nullptr) {
        return;
    }

    pReadStatus->isDroppedRecordCountValid = consumeDroppedRecordCount(pReadStatus->droppedRecordCount);
    pReadStatus->hasDataToRead = queryCall ? anyRecordFound : (stopReason != StopReason::drained);
}

ze_result_t LinuxInfoLogInstanceImp::collect(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer, uint32_t *pRecordCount,
                                             zes_intel_info_log_metadata_exp *pDescriptors,
                                             zes_intel_info_log_read_status_exp_t *pReadStatus, bool consuming) {
    if (*pSize == 0 || *pRecordCount == 0) {
        uint32_t totalSize = 0;
        uint32_t totalCount = 0;
        bool anyFound = false;
        ze_result_t result = queryRecords(&totalSize, &totalCount, anyFound);
        if (result != ZE_RESULT_SUCCESS) {
            *pSize = 0;
            *pRecordCount = 0;
            return result;
        }

        *pSize = totalSize;
        *pRecordCount = totalCount;
        fillReadStatus(pReadStatus, StopReason::drained, true, anyFound);
        return ZE_RESULT_SUCCESS;
    }

    if (consuming && tracePipeFd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): trace_pipe is not open, returning error: 0x%x\n",
                     NEO_FUNCTION_NAME, ZE_RESULT_ERROR_NOT_AVAILABLE);
        *pSize = 0;
        *pRecordCount = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t deadlineMs = (timeout == UINT64_MAX) ? UINT64_MAX : getCurrentTimeInMs() + timeout;
    StopReason stopReason = StopReason::drained;

    ze_result_t result = consuming
                             ? extractFromTracePipe(deadlineMs, pSize, pBuffer, pRecordCount, pDescriptors, stopReason)
                             : extractFromTraceSnapshot(deadlineMs, pSize, pBuffer, pRecordCount, pDescriptors, stopReason);

    // A record which did not fit is retained rather than dropped, so a size limited stop is a plain
    // success and is reported to the caller through 'hasDataToRead'.
    fillReadStatus(pReadStatus, stopReason, false, *pRecordCount != 0);

    return result;
}

ze_result_t LinuxInfoLogInstanceImp::readWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                      uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                      zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return collect(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus, true);
}

ze_result_t LinuxInfoLogInstanceImp::peekWithMetadata(uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                      uint32_t *pRecordCount, zes_intel_info_log_metadata_exp *pDescriptors,
                                                      zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return collect(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus, false);
}

} // namespace Sysman
} // namespace L0
