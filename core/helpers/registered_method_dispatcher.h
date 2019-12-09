/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <functional>

namespace NEO {

template <typename MethodArgsT, typename EstimateMethodArgsT>
class RegisteredMethodDispatcher {
  public:
    using CommandsSizeEstimationMethodT = std::function<EstimateMethodArgsT>;
    using RegisteredMethodT = std::function<MethodArgsT>;

    void registerMethod(RegisteredMethodT method) {
        this->method = method;
    }

    void registerCommandsSizeEstimationMethod(CommandsSizeEstimationMethodT method) {
        this->commandsEstimationMethod = method;
    }

    template <typename... Args>
    void operator()(Args &&... args) const {
        if (method) {
            method(std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    size_t estimateCommandsSize(Args &&... args) const {
        if (commandsEstimationMethod) {
            return commandsEstimationMethod(std::forward<Args>(args)...);
        }
        return 0;
    }

  protected:
    CommandsSizeEstimationMethodT commandsEstimationMethod;
    RegisteredMethodT method;
};

} // namespace NEO
