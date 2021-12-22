/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/module/module.h"

#include <memory>

namespace L0 {

struct ModuleBuildLogImp : public ModuleBuildLog {
    ModuleBuildLogImp() {}

    ~ModuleBuildLogImp() override {}

    ze_result_t destroy() override {
        delete this;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t getString(size_t *pSize, char *pBuildLog) override {
        const size_t requiredSize{buildLog.size() + 1};
        if (pBuildLog == nullptr) {
            *pSize = requiredSize;
            return ZE_RESULT_SUCCESS;
        }

        if (*pSize < requiredSize) {
            return ZE_RESULT_ERROR_INVALID_SIZE;
        }

        memcpy_s(pBuildLog, *pSize, buildLog.c_str(), buildLog.size());
        pBuildLog[buildLog.size()] = '\0';

        *pSize = requiredSize;

        return ZE_RESULT_SUCCESS;
    }

    void appendString(const char *pBuildLog, size_t size) override {
        if ((pBuildLog == nullptr) || (size == 0) || (pBuildLog[0] == '\0'))
            return;

        if (pBuildLog[size - 1] == '\0')
            --size;

        if (this->buildLog.length() != 0)
            this->buildLog.append("\n");

        this->buildLog.append(pBuildLog, size);
    }

  protected:
    std::string buildLog;
};

ModuleBuildLog *ModuleBuildLog::create() {
    auto moduleBuildLog = new ModuleBuildLogImp();
    UNRECOVERABLE_IF(moduleBuildLog == nullptr);
    return moduleBuildLog;
}

} // namespace L0
