/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/variable.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::MCL::Variable>
    : public ::L0::MCL::Variable {

    using BaseClass = ::L0::MCL::Variable;
    using BaseClass::addCommitVariableToBaseCmdList;
    using BaseClass::addKernelArgUsageImmediateAsChunk;
    using BaseClass::addKernelArgUsageImmediateAsContinuous;
    using BaseClass::bufferUsages;
    using BaseClass::cmdList;
    using BaseClass::desc;
    using BaseClass::eventValue;
    using BaseClass::hasKernelArgCorrectType;
    using BaseClass::immediateValueChunks;
    using BaseClass::kernelDispatch;
    using BaseClass::setBufferVariable;
    using BaseClass::setCbWaitEventUpdateOperation;
    using BaseClass::setCommitVariable;
    using BaseClass::setGlobalOffsetVariable;
    using BaseClass::setGroupCountVariable;
    using BaseClass::setGroupSizeVariable;
    using BaseClass::setSignalEventVariable;
    using BaseClass::setSlmBufferVariable;
    using BaseClass::setValueVariable;
    using BaseClass::setValueVariableContinuous;
    using BaseClass::setValueVariableInChunks;
    using BaseClass::setWaitEventVariable;
    using BaseClass::slmValue;
    using BaseClass::usedInDispatch;
    using BaseClass::valueUsages;

    WhiteBox() : ::L0::MCL::Variable(nullptr, "") {}
};

using Variable = WhiteBox<::L0::MCL::Variable>;

} // namespace ult
} // namespace L0
