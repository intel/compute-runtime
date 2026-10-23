/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_variables_listener.h"

#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include <utility>
#include <vector>

#ifndef NEO_ULT_CHECK_DEBUG_VARIABLES_PER_TEST
#define NEO_ULT_CHECK_DEBUG_VARIABLES_PER_TEST 0
#endif

namespace NEO {

// The snapshot follows the leaked value, so the leak is reported once.
template <typename T>
static void expectDebugVariableRestored(const char *variableName, T &snapshot, const T &current) {
    if (snapshot != current) {
        EXPECT_EQ(snapshot, current) << "debug variable " << variableName << " was not restored";
        snapshot = current;
    }
}

template <typename T>
static void expectDebugVariableValueRestored(const char *variableName, DebugVarBase<T> &snapshot, const DebugVarBase<T> &current, const char *hint) {
    if (snapshot.getRef() != current.getRef()) {
        EXPECT_EQ(snapshot.getRef(), current.getRef()) << "debug variable " << variableName << " was not restored" << hint;
        snapshot.set(current.getRef());
    }
}

static std::vector<std::pair<const void *, const char *>> debugVariableNames;

static void initDebugVariableNames() {
    if (!debugVariableNames.empty()) {
        return;
    }
#define ADD_DEBUG_VARIABLE_NAME(variableName) debugVariableNames.emplace_back(&debugManager.flags.variableName, #variableName);
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) ADD_DEBUG_VARIABLE_NAME(variableName)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#define DECLARE_RAW_ENV_SCOPED_V(dataType, variableName, envVarName, defaultValue, scope, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) ADD_DEBUG_VARIABLE_NAME(variableName)
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_SCOPED_V
#undef DECLARE_RAW_ENV_VARIABLE
#undef ADD_DEBUG_VARIABLE_NAME
}

static const char *getDebugVariableName(const void *variable) {
    for (const auto &[address, name] : debugVariableNames) {
        if (address == variable) {
            return name;
        }
    }
    return "<unknown>";
}

static void reportDebugVariableChange(const void *variable) {
    ADD_FAILURE() << "debug variable " << getDebugVariableName(variable) << " was changed in the test without DebugManagerStateRestore";
}

static void expectDebugVariablesRestored(DebugVariables &debugVarSnapshot) {
    if (debugVarSnapshot == debugManager.flags) {
        return;
    }
    const char *hint = NEO_ULT_CHECK_DEBUG_VARIABLES_PER_TEST ? "" : ", it was changed outside of a test or without set(), to find where set NEO_ULT_CHECK_DEBUG_VARIABLES_PER_TEST to 1 in shared/test/common/helpers/debug_variables_listener.cpp";
#define EXPECT_VARIABLE_RESTORED(variableName) expectDebugVariableValueRestored(#variableName, debugVarSnapshot.variableName, debugManager.flags.variableName, hint);
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) EXPECT_VARIABLE_RESTORED(variableName)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#define DECLARE_RAW_ENV_SCOPED_V(dataType, variableName, envVarName, defaultValue, scope, description) EXPECT_VARIABLE_RESTORED(variableName)
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) EXPECT_VARIABLE_RESTORED(variableName)
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_SCOPED_V
#undef DECLARE_RAW_ENV_VARIABLE
#undef EXPECT_VARIABLE_RESTORED
}

void DebugVariablesListener::OnTestIterationStart(const ::testing::UnitTest &, int) {
    initDebugVariableNames();
    debugVarSnapshot = debugManager.flags;
    injectFcnSnapshot = debugManager.injectFcn;
}

void DebugVariablesListener::OnTestStart(const ::testing::TestInfo &) {
    DebugVariablesChangeTracker::startRecording(debugManager.flags);
}

void DebugVariablesListener::OnTestEnd(const ::testing::TestInfo &) {
    DebugVariablesChangeTracker::stopRecording(&reportDebugVariableChange);
#if NEO_ULT_CHECK_DEBUG_VARIABLES_PER_TEST
    expectDebugVariablesRestored(debugVarSnapshot);
#endif
    expectDebugVariableRestored("injectFcn", injectFcnSnapshot, debugManager.injectFcn);
}

void DebugVariablesListener::OnTestIterationEnd(const ::testing::UnitTest &, int) {
    expectDebugVariablesRestored(debugVarSnapshot);
}

} // namespace NEO
