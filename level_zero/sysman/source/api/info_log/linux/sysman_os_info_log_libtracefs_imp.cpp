/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_imp.h"
#include "level_zero/sysman/source/api/info_log/linux/sysman_os_info_log_instance_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <unistd.h>
#include <vector>

namespace L0 {
namespace Sysman {

const std::vector<std::string> tracefsPaths = {"/sys/kernel/tracing", "/sys/kernel/debug/tracing"};
static const std::string xeErrorCperTracepointPath = "events/xe/xe_error_cper";

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
    pProperties->isNamedInstancedCollectionSupported = isNamedInstancedCollectionAvailable();
    pProperties->isPeekSupported = isPeekAvailable();

    return ZE_RESULT_SUCCESS;
}

bool LinuxInfoLogImp::isNamedInstancedCollectionAvailable() {
    return true;
}

bool LinuxInfoLogImp::isPeekAvailable() {
    return true;
}

// Directory holding the named tracefs instances of the first tracefs mount point which offers them.
// Empty when no mount point does, which is the case on a kernel built without tracefs instances.
std::string LinuxInfoLogImp::getTracefsInstancesDirPath() {
    int errorNum = 0;
    for (const auto &tracingDir : tracefsPaths) {
        std::string instancesDir = tracingDir + "/instances";
        if (SysmanSysCallsWrapper::access(instancesDir.c_str(), F_OK, errorNum) == 0) {
            return instancesDir;
        }
    }
    return "";
}

bool LinuxInfoLogImp::checkInstancePreExists(const char *instanceName) {
    std::string instancesDir = getTracefsInstancesDirPath();
    if (instancesDir.empty()) {
        return false;
    }

    int errorNum = 0;
    std::string instanceDir = instancesDir + "/" + instanceName;
    return SysmanSysCallsWrapper::access(instanceDir.c_str(), F_OK, errorNum) == 0;
}

// A tracefs instance records nothing about who created it, so an advisory lock on its directory is
// what marks it as belonging to this API. The lock only ever contends with another sysman consumer,
// because nothing else takes it: an instance provisioned by a user or by another tracing tool is
// therefore reused rather than rejected, which is the whole point of the distinction. The lock is
// held for as long as the collection instance is alive and the kernel drops it if the process dies,
// so a consumer which crashed does not keep the name reserved for ever.
ze_result_t LinuxInfoLogImp::claimInstanceOwnership(const char *instanceName, int &ownershipFd) {
    std::string instancesDir = getTracefsInstancesDirPath();
    if (instancesDir.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to locate the tracefs instances directory\n", NEO_FUNCTION_NAME);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    int errorNum = 0;
    std::string instanceDir = instancesDir + "/" + instanceName;
    int fd = SysmanSysCallsWrapper::open(instanceDir.c_str(), O_RDONLY | O_DIRECTORY, errorNum);
    if (fd < 0) {
        ze_result_t result = LinuxSysmanImp::getResult(errorNum);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to open the directory of tracefs instance '%s', returning error: 0x%x\n",
                     NEO_FUNCTION_NAME, instanceName, result);
        return result;
    }

    // The lock is requested without blocking, so the only outcome to expect of a failure here is that
    // it is already held. Refusing is in any case the safe answer to ownership which could not be
    // established, because collecting from an instance another consumer owns would split the records
    // between the two of them.
    if (SysmanSysCallsWrapper::flock(fd, LOCK_EX | LOCK_NB, errorNum) != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Tracefs instance '%s' is already owned by another sysman collection instance, errno: %d\n",
                     NEO_FUNCTION_NAME, instanceName, errorNum);
        SysmanSysCallsWrapper::close(fd, errorNum);
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    ownershipFd = fd;
    return ZE_RESULT_SUCCESS;
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

ze_result_t LinuxInfoLogImp::createInstance(const char *pInstanceName,
                                            zes_intel_info_log_instance_exp_desc_t *pDesc,
                                            std::unique_ptr<OsInfoLogInstance> &pOsInfoLogInstance) {
    struct tracefs_instance *pTraceFsInstance = nullptr;
    bool preExisting = false;
    int ownershipFd = -1;

    if (pInstanceName != nullptr) {
        preExisting = checkInstancePreExists(pInstanceName);

        pTraceFsInstance = pTraceFsApi->traceFsInstanceCreate(pInstanceName);
        if (pTraceFsInstance == nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to create tracefs instance '%s'\n", NEO_FUNCTION_NAME, pInstanceName);
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        // Claimed before the buffer is reconfigured, because resizing a tracefs buffer discards what it
        // holds: a create which is going to be refused must not throw away the records of the consumer
        // which owns the instance. The tracefs instance is released without being destroyed on refusal,
        // whatever this call concluded about it pre-existing, since the owner is still collecting from it.
        ze_result_t ownershipResult = claimInstanceOwnership(pInstanceName, ownershipFd);
        if (ownershipResult != ZE_RESULT_SUCCESS) {
            pTraceFsApi->traceFsInstanceFree(pTraceFsInstance);
            return ownershipResult;
        }
    }

    // The tracepoint and tracing_on state is sampled before anything is changed, so that teardown
    // only reverts what this instance turned on.
    bool eventWasAlreadyEnabled = checkEventEnabled(pTraceFsInstance);
    bool tracingWasAlreadyOn = checkTracingOn(pTraceFsInstance);

    auto pInstance = std::make_unique<LinuxInfoLogInstanceImp>(pTraceFsApi.get(), infoLogFormat, pTraceFsInstance,
                                                               (pInstanceName != nullptr) ? pInstanceName : "",
                                                               preExisting, eventWasAlreadyEnabled, tracingWasAlreadyOn,
                                                               ownershipFd);

    ze_result_t result = pInstance->applyBufferConfiguration(pDesc);
    if (result == ZE_RESULT_SUCCESS) {
        result = pInstance->startCollection();
    }

    if (result != ZE_RESULT_SUCCESS) {
        // Letting pInstance go out of scope runs its teardown, which reverts the collection state
        // and releases the tracefs instance.
        return result;
    }

    pOsInfoLogInstance = std::move(pInstance);
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
