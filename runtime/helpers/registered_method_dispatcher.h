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
template <typename ArgsT>
class RegisteredMethodDispatcher {
  public:
    using CommandsSizeEstimationMethodT = std::function<size_t(void)>;
    using RegisteredMethodT = std::function<ArgsT>;

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

    size_t estimateCommandsSize() const {
        if (commandsEstimationMethod) {
            return commandsEstimationMethod();
        }
        return 0;
    }

  protected:
    CommandsSizeEstimationMethodT commandsEstimationMethod;
    RegisteredMethodT method;
};

} // namespace NEO
