/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

DirectSubmissionDiagnosticsCollector::DirectSubmissionDiagnosticsCollector(uint32_t executions,
                                                                           bool storeExecutions,
                                                                           int32_t ringBufferLogData,
                                                                           int32_t semaphoreLogData,
                                                                           int32_t workloadMode,
                                                                           bool cacheFlushLog,
                                                                           bool monitorFenceLog)
    : storeExecutions(storeExecutions) {
    UNRECOVERABLE_IF(executions == 0);
    executionList.resize(executions);
    executionsCount = executions;
    std::stringstream value;
    value << std::dec << "mode-" << workloadMode << "_executions-" << executions;
    value << "_ring_" << ringBufferLogData << "_semaphore_" << semaphoreLogData;
    value << "_cacheflush-" << cacheFlushLog << "_monitorfence-" << monitorFenceLog;
    std::stringstream filename;
    filename << "ulls_diagnostic_" << value.str() << ".log";
    logFile = IoFunctions::fopenPtr(filename.str().c_str(), "at");
    UNRECOVERABLE_IF(logFile == nullptr);
    IoFunctions::fprintf(logFile, "%s\n", value.str().c_str());
}

void DirectSubmissionDiagnosticsCollector::storeData() {
    auto initDelta = diagnosticModeDiagnosticTime - diagnosticModeAllocationTime;
    int64_t initTimeDiff =
        std::chrono::duration_cast<std::chrono::microseconds>(initDelta).count();

    IoFunctions::fprintf(logFile, "From allocations ready to exit of OS submit function %lld useconds\n", initTimeDiff);

    if (storeExecutions) {
        for (uint32_t execution = 0; execution < executionsCount; execution++) {
            DirectSubmissionSingleDelta &delta = executionList[execution];
            std::stringstream value;
            value << std::dec << " execution: " << execution;
            value << " total diff: " << delta.totalTimeDiff << " nsec"
                  << " dispatch-submit: " << delta.dispatchSubmitTimeDiff << " nsec"
                  << " submit-wait: " << delta.submitWaitTimeDiff << " nsec";
            IoFunctions::fprintf(logFile, "%s\n", value.str().c_str());
        }
    }
}

} // namespace NEO
