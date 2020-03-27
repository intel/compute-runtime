/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"

#include "shared/source/helpers/debug_helpers.h"

#include <ctime>
#include <iomanip>

namespace NEO {
DirectSubmissionDiagnosticsCollector::DirectSubmissionDiagnosticsCollector(uint32_t executions, bool storeExecutions)
    : storeExecutions(storeExecutions) {
    UNRECOVERABLE_IF(executions == 0);
    executionList.resize(executions);
    executionsCount = executions;

    std::chrono::system_clock::time_point timeStamp = std::chrono::system_clock::now();
    std::time_t dateTime = std::chrono::system_clock::to_time_t(timeStamp);

    std::stringstream value;
    value << std::dec << "executions-" << executions;
    value << "_time-" << std::put_time(std::localtime(&dateTime), "%F-%T");
    std::stringstream filename;
    filename << "ulls_diagnostic_" << value.str() << ".log";
    logFile = IoFunctions::fopenPtr(filename.str().c_str(), "at");
    UNRECOVERABLE_IF(logFile == nullptr);
    IoFunctions::fprintf(logFile, "%s\n", value.str().c_str());
}

void DirectSubmissionDiagnosticsCollector::storeData() {
    auto initDelta = diagnosticModeDiagnosticTime - diagnosticModeAllocationTime;
    int64_t initTimeDiff =
        std::chrono::duration_cast<std::chrono::nanoseconds>(initDelta).count();

    IoFunctions::fprintf(logFile, "From allocations ready to exit of OS submit function %lld\n", initTimeDiff);

    if (storeExecutions) {
        for (uint32_t execution = 0; execution < executionsCount; execution++) {
            DirectSubmissionSingleDelta delta = executionList.at(execution);
            std::stringstream value;
            value << std::dec << " execution: " << execution;
            value << " total diff: " << delta.totalTimeDiff
                  << " dispatch-submit: " << delta.dispatchSubmitTimeDiff << " submit-wait: " << delta.submitWaitTimeDiff;
            IoFunctions::fprintf(logFile, "%s\n", value.str().c_str());
        }
    }
}

} // namespace NEO
