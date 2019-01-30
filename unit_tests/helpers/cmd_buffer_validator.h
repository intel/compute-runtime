/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/utilities/stackvec.h"
#include "unit_tests/helpers/hw_parse.h"

#include <memory>
#include <string>
#include <vector>

namespace OCLRT {

struct CmdValidator {
    CmdValidator() {
    }
    virtual ~CmdValidator() = default;
    virtual bool operator()(GenCmdList::iterator it, size_t numInSection, const std::string &member, std::string &outFailReason) = 0;
};

template <typename ChildT>
struct CmdValidatorWithStaticStorage : CmdValidator {
    static ChildT *get() {
        static ChildT val;
        return &val;
    }
};

template <typename CmdT, typename ReturnT, ReturnT (CmdT::*Getter)() const, ReturnT Expected>
struct GenericCmdValidator : CmdValidatorWithStaticStorage<GenericCmdValidator<CmdT, ReturnT, Getter, Expected>> {
    bool operator()(GenCmdList::iterator it, size_t numInSection, const std::string &member, std::string &outFailReason) override {
        auto cmd = genCmdCast<CmdT *>(*it);
        UNRECOVERABLE_IF(cmd == nullptr);
        if (Expected != (cmd->*Getter)()) {
            outFailReason = member + " - expected: " + std::to_string(Expected) + ", got: " + std::to_string((cmd->*Getter)());
            return false;
        }
        return true;
    }
};

struct NamedValidator {
    NamedValidator(CmdValidator *validator)
        : validator(validator), name("Unspecified") {
    }

    NamedValidator(CmdValidator *validator, const char *name)
        : validator(validator), name(name) {
    }

    CmdValidator *validator;
    const char *name;
};

#define EXPECT_MEMBER(TYPE, FUNC, EXPECTED) \
    NamedValidator { GenericCmdValidator<TYPE, decltype((((TYPE *)nullptr)->FUNC)()), &TYPE::FUNC, EXPECTED>::get(), #FUNC }

using Expects = std::vector<NamedValidator>;

struct MatchCmd {
    MatchCmd(int amount, bool matchesAny)
        : amount(amount), matchesAny(matchesAny) {
    }

    MatchCmd(int amount)
        : amount(amount), matchesAny(false) {
    }

    virtual ~MatchCmd() = default;

    virtual bool matches(GenCmdList::iterator it) const = 0;
    virtual bool validates(GenCmdList::iterator it, std::string &outReason) const = 0;
    virtual const char *getName() const = 0;
    virtual void capture(GenCmdList::iterator it) = 0;

    int getExpectedCount() const {
        return amount;
    }

    bool getMatchesAny() const {
        return matchesAny;
    }

  protected:
    int amount = 0;
    bool matchesAny = false;
};

constexpr int32_t AnyNumber = -1;
constexpr int32_t AtLeastOne = -2;
inline std::string countToString(int32_t count) {
    if (count == AnyNumber) {
        return "AnyNumber";
    } else if (count == AtLeastOne) {
        return "AtLeastOne";
    } else {
        return std::to_string(count);
    }
}

inline bool notPreciseNumber(int32_t count) {
    return (count == AnyNumber) || (count == AtLeastOne);
}

struct MatchAnyCmd : MatchCmd {
    MatchAnyCmd(int amount)
        : MatchCmd(amount, true) {
        if (amount > 0) {
            captured.reserve(amount);
        }
    }
    bool matches(GenCmdList::iterator it) const override {
        return true;
    }
    bool validates(GenCmdList::iterator it, std::string &outReason) const override {
        return true;
    }
    void capture(GenCmdList::iterator it) override {
        captured.push_back(*it);
    }
    const char *getName() const override {
        return "AnyCommand";
    }

  protected:
    StackVec<const void *, 16> captured;
};

template <typename FamilyType, typename CmdType>
struct MatchHwCmd : MatchCmd {
    MatchHwCmd(int amount)
        : MatchCmd(amount) {
        if (amount > 0) {
            captured.reserve(amount);
        }
    }

    MatchHwCmd(int amount, Expects &&validators)
        : MatchHwCmd(amount) {
        this->validators.swap(validators);
    }

    bool matches(GenCmdList::iterator it) const override {
        return nullptr != genCmdCast<CmdType *>(*it);
    }

    bool validates(GenCmdList::iterator it, std::string &outReason) const override {
        for (auto &v : validators) {
            if (false == (*v.validator)(it, captured.size(), v.name, outReason)) {
                return false;
            }
        }
        return true;
    }

    void capture(GenCmdList::iterator it) override {
        UNRECOVERABLE_IF(false == matches(it));
        UNRECOVERABLE_IF(captured.size() == static_cast<size_t>(amount));
        captured.push_back(genCmdCast<CmdType *>(*it));
    }

    const char *getName() const override {
        CmdType cmd;
        cmd.init();
        return HardwareParse::getCommandName<FamilyType>(&cmd);
    }

  protected:
    StackVec<const CmdType *, 16> captured;
    Expects validators;
};

template <typename FamilyType>
inline bool expectCmdBuff(GenCmdList::iterator begin, GenCmdList::iterator end,
                          std::vector<MatchCmd *> &&expectedCmdBuffMatchers, std::string *outReason = nullptr) {
    if (expectedCmdBuffMatchers.size() == 0) {
        return begin == end;
    }
    bool failed = false;
    std::string failReason;
    auto it = begin;
    int cmdNum = 0;
    size_t currentMatcher = 0;
    int currentMatcherCount = 0;
    StackVec<std::pair<const char *, bool>, 32> matchedCommandNames;
    auto matchedCommandsString = [&]() -> std::string {
        if (matchedCommandNames.size() == 0) {
            return "EMPTY";
        }
        std::string ret = "";
        for (size_t i = 0; i < matchedCommandNames.size(); ++i) {
            if (matchedCommandNames[i].second) {
                ret += std::to_string(i) + ":ANY(" + matchedCommandNames[i].first + ") ";
            } else {
                ret += std::to_string(i) + ":" + matchedCommandNames[i].first + " ";
            }
        }
        return ret;
    };
    while (it != end) {
        if (currentMatcher < expectedCmdBuffMatchers.size()) {
            auto currentMatcherExpectedCount = expectedCmdBuffMatchers[currentMatcher]->getExpectedCount();
            if (expectedCmdBuffMatchers[currentMatcher]->getMatchesAny() && ((currentMatcherExpectedCount == AnyNumber) || ((currentMatcherExpectedCount == AtLeastOne) && (currentMatcherCount > 0)))) {
                if (expectedCmdBuffMatchers.size() > currentMatcher + 1) {
                    // eat as many as possible but proceed to next matcher when possible
                    if (expectedCmdBuffMatchers[currentMatcher + 1]->matches(it)) {
                        ++currentMatcher;
                        currentMatcherCount = 0;
                    }
                }
            } else if ((notPreciseNumber(expectedCmdBuffMatchers[currentMatcher]->getExpectedCount())) && (false == expectedCmdBuffMatchers[currentMatcher]->matches(it))) {
                // proceed to next matcher if not matched
                if ((expectedCmdBuffMatchers[currentMatcher]->getExpectedCount() == AtLeastOne) && (currentMatcherCount < 1)) {
                    failed = true;
                    failReason = "Unmatched cmd#" + std::to_string(cmdNum) + " - expected " + std::string(expectedCmdBuffMatchers[currentMatcher]->getName()) + "(" + countToString(expectedCmdBuffMatchers[currentMatcher]->getExpectedCount()) + " - " + std::to_string(currentMatcherCount) + ") after : " + matchedCommandsString();
                    break;
                }
                ++currentMatcher;
                currentMatcherCount = 0;
            }

            while ((currentMatcher < expectedCmdBuffMatchers.size()) && expectedCmdBuffMatchers[currentMatcher]->getExpectedCount() == 0) {
                if (expectedCmdBuffMatchers[currentMatcher]->matches(it)) {
                    failed = true;
                    failReason = "Unmatched cmd#" + std::to_string(cmdNum) + " - expected anything but " + std::string(expectedCmdBuffMatchers[currentMatcher]->getName()) + "(" + countToString(expectedCmdBuffMatchers[currentMatcher]->getExpectedCount()) + " - " + std::to_string(currentMatcherCount) + ") after : " + matchedCommandsString();
                    break;
                }
                ++currentMatcher;
                currentMatcherCount = 0;
            }
        }

        if (currentMatcher >= expectedCmdBuffMatchers.size()) {
            failed = true;
            std::string unmatchedCommands;
            while (it != end) {
                unmatchedCommands += std::to_string(cmdNum) + ":" + HardwareParse::getCommandName<FamilyType>(*it) + " ";
                ++it;
                ++cmdNum;
            }
            failReason = "Unexpected commands at the end of the command buffer : " + unmatchedCommands + ", AFTER : " + matchedCommandsString();
            break;
        }

        if (false == expectedCmdBuffMatchers[currentMatcher]->matches(it)) {
            failed = true;
            failReason = "Unmatched cmd#" + std::to_string(cmdNum) + " - expected " + std::string(expectedCmdBuffMatchers[currentMatcher]->getName()) + "(" + countToString(expectedCmdBuffMatchers[currentMatcher]->getExpectedCount()) + " - " + std::to_string(currentMatcherCount) + ") after : " + matchedCommandsString();
            break;
        }

        if (false == expectedCmdBuffMatchers[currentMatcher]->validates(it, failReason)) {
            failReason = "cmd#" + std::to_string(cmdNum) + " (" + HardwareParse::getCommandName<FamilyType>(*it) + ") failed validation - reason : " + failReason + " after : " + matchedCommandsString();
            failed = true;
            break;
        }

        matchedCommandNames.push_back(std::make_pair(HardwareParse::getCommandName<FamilyType>(*it), expectedCmdBuffMatchers[currentMatcher]->getMatchesAny()));

        ++currentMatcherCount;
        if (currentMatcherCount == expectedCmdBuffMatchers[currentMatcher]->getExpectedCount()) {
            ++currentMatcher;
            currentMatcherCount = 0;
        }

        ++cmdNum;
        ++it;
    }

    if (failed == false) {
        while ((currentMatcher < expectedCmdBuffMatchers.size()) && ((expectedCmdBuffMatchers[currentMatcher]->getExpectedCount() == 0) || (expectedCmdBuffMatchers[currentMatcher]->getExpectedCount() == AnyNumber))) {
            ++currentMatcher;
            currentMatcherCount = 0;
        }

        if (currentMatcher == expectedCmdBuffMatchers.size()) {
            // no more matchers
        } else if (currentMatcher + 1 == expectedCmdBuffMatchers.size()) {
            // last matcher
            auto currentMatcherExpectedCount = expectedCmdBuffMatchers[currentMatcher]->getExpectedCount();
            if ((currentMatcherExpectedCount == AtLeastOne) && (currentMatcherCount < 1)) {
                failReason = "Unexpected command buffer end at cmd#" + std::to_string(cmdNum) + " - expected " + expectedCmdBuffMatchers[currentMatcher]->getName() + "(" + countToString(currentMatcherExpectedCount) + " - " + std::to_string(currentMatcherCount) + ") after : " + matchedCommandsString();
                failed = true;
            }
            if ((false == notPreciseNumber(currentMatcherExpectedCount)) && (currentMatcherExpectedCount != currentMatcherCount)) {
                failReason = "Unexpected command buffer end at cmd#" + std::to_string(cmdNum) + " - expected " + expectedCmdBuffMatchers[currentMatcher]->getName() + "(" + countToString(currentMatcherExpectedCount) + " - " + std::to_string(currentMatcherCount) + ") after : " + matchedCommandsString();
                failed = true;
            }
        } else {
            // many matchers left
            std::string expectedMatchers = "";
            int32_t currentMatcherExpectedCount = expectedCmdBuffMatchers[currentMatcher]->getExpectedCount();
            expectedMatchers = expectedCmdBuffMatchers[currentMatcher]->getName() + std::string("(") + countToString(currentMatcherExpectedCount) + " - " + std::to_string(currentMatcherCount) + "), ";
            ++currentMatcher;
            while (currentMatcher < expectedCmdBuffMatchers.size()) {
                currentMatcherExpectedCount = expectedCmdBuffMatchers[currentMatcher]->getExpectedCount();
                expectedMatchers += expectedCmdBuffMatchers[currentMatcher]->getName() + std::string("(") + countToString(currentMatcherExpectedCount) + " - 0), ";
                ++currentMatcher;
            }
            failReason = "Unexpected command buffer end at cmd#" + std::to_string(cmdNum) + " - expected " + expectedMatchers + " after : " + matchedCommandsString();
            failed = true;
        }
    }

    if (failed) {
        if (outReason != nullptr) {
            *outReason = failReason;
        }
    }

    for (auto *matcher : expectedCmdBuffMatchers) {
        delete matcher;
    }

    return (failed == false);
}

template <typename FamilyType>
inline bool expectCmdBuff(OCLRT::LinearStream &commandStream, size_t startOffset,
                          std::vector<MatchCmd *> &&expectedCmdBuffMatchers, std::string *outReason = nullptr) {
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, startOffset);
    return expectCmdBuff<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), std::move(expectedCmdBuffMatchers), outReason);
}

} // namespace OCLRT
