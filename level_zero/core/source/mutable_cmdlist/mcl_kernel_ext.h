/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/kernel/kernel_imp.h"

#include <vector>
namespace L0::MCL {
struct Variable;

class MclKernelExt : public L0::KernelExt, NEO::NonCopyableAndNonMovableClass {
  public:
    static constexpr uint32_t extensionType = 0xff00abcd;

    MclKernelExt() = delete;

    MclKernelExt(size_t numArgs) { varArgs.resize(numArgs); }
    ~MclKernelExt() override {}

    inline void setGroupSizeVariable(Variable *groupSize) { this->groupSize = groupSize; }
    inline Variable *getGroupSizeVariable() const { return groupSize; }

    inline void setArgumentVariable(uint32_t argIndex, Variable *var) { varArgs[argIndex] = var; }
    inline const std::vector<Variable *> &getVariables() const { return varArgs; }
    void cleanArgumentVariables() {
        for (auto &var : varArgs) {
            var = nullptr;
        }
    }

    static MclKernelExt &get(L0::Kernel *kernel) {
        auto ext = static_cast<L0::KernelImp *>(kernel)->getExtension(MclKernelExt::extensionType);
        UNRECOVERABLE_IF(nullptr == ext);
        return *static_cast<MclKernelExt *>(ext);
    }

  protected:
    std::vector<Variable *> varArgs = {};
    Variable *groupSize = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<MclKernelExt>);

} // namespace L0::MCL
