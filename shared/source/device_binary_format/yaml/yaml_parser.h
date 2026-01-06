/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"
#include "shared/source/utilities/stackvec.h"

#include <array>
#include <iterator>
#include <string>

namespace NEO {

namespace Yaml {

constexpr bool isWhitespace(char c) {
    switch (c) {
    default:
        return false;
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        return true;
    }
}

constexpr bool isSeparationWhitespace(char c) {
    switch (c) {
    default:
        return false;
    case ' ':
    case '\t':
        return true;
    }
}

constexpr bool isLetter(char c) {
    return ((c >= 'a') & (c <= 'z')) || ((c >= 'A') & (c <= 'Z'));
}

constexpr bool isNumber(char c) {
    return ((c >= '0') & (c <= '9'));
}

constexpr bool isAlphaNumeric(char c) {
    return isLetter(c) || isNumber(c);
}

constexpr bool isHexDigit(char c) {
    return isNumber(c) || ((c >= 'a') & (c <= 'f')) || ((c >= 'A') & (c <= 'F'));
}

constexpr bool isHexPrefix(const char *parsePos, const char *parseEnd) {
    return (parseEnd - parsePos >= 2) && (parsePos[0] == '0') && (parsePos[1] == 'x' || parsePos[1] == 'X');
}

constexpr bool isNameIdentifierCharacter(char c) {
    return isAlphaNumeric(c) || ('_' == c) || ('-' == c) || ('.' == c);
}

constexpr bool isNameIdentifierBeginningCharacter(char c) {
    return isLetter(c) || ('_' == c);
}

constexpr bool isSign(char c) {
    return ('+' == c) || ('-' == c);
}

inline bool isSpecificNameIdentifier(ConstStringRef wholeText, const char *parsePos, ConstStringRef pattern) {
    UNRECOVERABLE_IF(parsePos < wholeText.begin());
    bool hasEnoughText = (reinterpret_cast<uintptr_t>(parsePos) + pattern.size() <= reinterpret_cast<uintptr_t>(wholeText.end()));
    bool isEnd = (reinterpret_cast<uintptr_t>(parsePos) + pattern.size() == reinterpret_cast<uintptr_t>(wholeText.end()));
    bool matched = hasEnoughText &&
                   ((pattern == ConstStringRef(parsePos, pattern.size())) && (isEnd || (false == isNameIdentifierCharacter(parsePos[pattern.size()]))));
    return matched;
}

inline bool isMatched(ConstStringRef wholeText, const char *parsePos, ConstStringRef text) {
    UNRECOVERABLE_IF(parsePos < wholeText.begin());
    bool hasEnoughText = (reinterpret_cast<uintptr_t>(parsePos) + text.size() <= reinterpret_cast<uintptr_t>(wholeText.end()));
    return hasEnoughText && (text == ConstStringRef(parsePos, text.size()));
}

constexpr const char *consumeNumberOrSign(ConstStringRef wholeText, const char *parsePos, bool allowSign = true) {
    UNRECOVERABLE_IF(parsePos < wholeText.begin());
    UNRECOVERABLE_IF(parsePos == wholeText.end());
    auto parseEnd = wholeText.end();
    const auto isHex = isHexPrefix(parsePos, parseEnd);
    if (isNumber(*parsePos) || isHex) {
        auto it = parsePos;
        if (isHex) {
            it += 2;
        }
        while (it < parseEnd) {
            if (isHex) {
                if (!isHexDigit(*it)) {
                    break;
                }
            } else {
                if (!isNumber(*it) && (*it != '.')) {
                    break;
                }
            }
            ++it;
        }
        if (it < parseEnd) {
            return isLetter(*it) ? parsePos : it;
        } else {
            return it;
        }
    } else if (isSign(*parsePos) && allowSign) {
        if (parsePos + 1 < wholeText.end()) {
            return consumeNumberOrSign(wholeText, parsePos + 1, false);
        } else {
            return parsePos + 1;
        }
    }
    return parsePos;
}

constexpr const char *consumeNameIdentifier(ConstStringRef wholeText, const char *parsePos) {
    auto parseEnd = wholeText.end();
    if (isNameIdentifierBeginningCharacter(*parsePos)) {
        auto it = parsePos + 1;
        while (it < parseEnd) {
            if (false == isNameIdentifierCharacter(*it)) {
                if (false == isSeparationWhitespace(*it)) {
                    break;
                }
            }
            ++it;
        }
        return it;
    }
    return parsePos;
}

constexpr const char *consumeStringLiteral(ConstStringRef wholeText, const char *parsePos) {
    auto stringLiteralBeg = *parsePos;
    switch (stringLiteralBeg) {
    default:
        return parsePos;
    case '\'':
        break;
    case '\"':
        break;
    }
    auto parseEnd = wholeText.end();
    auto it = parsePos + 1;
    while (it < parseEnd) {
        if (stringLiteralBeg == it[0]) {
            if (it[-1] != '\\') { // allow escape characters
                break;
            }
        }
        ++it;
    }
    if (it == parseEnd) {
        return parsePos; // unterminated literal
    }
    return it + 1;
}

using TokenId = uint32_t;

constexpr TokenId invalidTokenId = std::numeric_limits<TokenId>::max();

struct Token {
    enum Type : uint8_t { identifier,
                          literalString,
                          literalNumber,
                          singleCharacter,
                          comment,
                          fileSectionBeg,
                          fileSectionEnd,
                          collectionBeg,
                          collectionEnd };

    constexpr Token(ConstStringRef tokData, Type tokType) {
        pos = tokData.begin();
        len = static_cast<uint32_t>(tokData.length());
        traits.type = tokType;
        traits.character0 = tokData[0];
    }

    const char *pos = nullptr;
    uint32_t len = 0U;
    struct {
        Type type = Token::Type::identifier;
        char character0 = 0U;
    } traits;

    constexpr ConstStringRef cstrref() const {
        return ConstStringRef(pos, len);
    }
};

static_assert(sizeof(Token) <= 16, "");

constexpr bool operator==(Token token, ConstStringRef matcher) {
    return (matcher[0] == token.traits.character0) && (ConstStringRef(token.pos + 1, token.len - 1) == ConstStringRef(matcher.begin() + 1, matcher.size() - 1));
}

constexpr bool operator==(ConstStringRef matcher, Token token) {
    return (token == matcher);
}

constexpr bool operator!=(Token token, ConstStringRef matcher) {
    return (false == (token == matcher));
}

constexpr bool operator!=(ConstStringRef matcher, Token token) {
    return token != matcher;
}

constexpr bool operator==(Token token, char matcher) {
    return ((matcher == token.traits.character0) & (token.len = 1U));
}

constexpr bool operator==(char matcher, Token token) {
    return token == matcher;
}

constexpr bool operator!=(Token token, char matcher) {
    return (false == (token == matcher));
}

constexpr bool operator!=(char matcher, Token token) {
    return token != matcher;
}

constexpr bool isVectorDataType(const Token &token) {
    auto tokenString = ConstStringRef(token.pos, token.len);
    constexpr std::array<const char *, 7> vectorAttributesNames = {
        "kernels",
        "functions",
        "global_host_access_table",
        "payload_arguments",
        "per_thread_payload_arguments",
        "binding_table_indices",
        "per_thread_memory_buffers"};
    for (const auto &type : vectorAttributesNames) {
        if (equals(tokenString, type)) {
            return true;
        }
    }
    return false;
}

constexpr bool allowEmptyVectorDataType(const Token &token) {
    auto tokenString = ConstStringRef(token.pos, token.len);
    if (equals(tokenString, "kernels")) {
        return true;
    }
    return false;
}

struct Line {
    enum class LineType : uint8_t { empty,
                                    comment,
                                    fileSection,
                                    dictionaryEntry,
                                    listEntry };
    TokenId first = 0U;
    TokenId last = 0U; // note : NOT past last (aka end)

    uint16_t indent = 0U;
    LineType lineType = LineType::empty;
    struct LineTraits {
        union {
            struct {
                bool hasInlineDataMarkers : 1;
                bool hasDictionaryEntry : 1;
            };
            uint8_t packed;
        };
        void reset() {
            this->packed = 0U;
        }
        constexpr LineTraits() : packed(0) {
        }
    } traits;

    constexpr Line(LineType lineType, uint16_t indent, TokenId first, TokenId last, LineTraits traits)
        : first(first), last(last), indent(indent), lineType(lineType), traits{} {
        this->traits = traits;
    }
};
static_assert(sizeof(Line) == 12, "");

template <typename T, typename It>
inline bool reserveBasedOnEstimates(T &container, It beg, It end, It pos) {
    if ((container.size() < container.capacity()) || (pos == beg)) {
        return false;
    }
    DEBUG_BREAK_IF((beg > end) || (pos < beg));
    auto normalizedPosInv = float(end - beg) / float(pos - beg);
    auto estimatedTotalElements = static_cast<size_t>(container.size() * normalizedPosInv);
    container.reserve(estimatedTotalElements);
    return true;
}

using TokensCache = StackVec<Token, 2048>;
using LinesCache = StackVec<Line, 512>;

std::string constructYamlError(size_t lineNumber, const char *lineBeg, const char *parsePos, const char *reason = nullptr);

bool isValidInlineCollectionFormat(const char *context, const char *contextEnd);
constexpr ConstStringRef inlineCollectionYamlErrorMsg = "NEO::Yaml : Inline collection is not in valid regex format - ^\\[(\\s*(\\d|\\w)+,?)*\\s*\\]\\s*\\n";

bool tokenize(ConstStringRef text, LinesCache &outLines, TokensCache &outTokens, std::string &outErrReason, std::string &outWarning);

using NodeId = uint32_t;
constexpr NodeId invalidNodeID = std::numeric_limits<NodeId>::max();

struct alignas(32) Node {
    TokenId key = invalidTokenId;
    TokenId value = invalidTokenId;
    NodeId id = invalidNodeID;
    NodeId parentId = invalidNodeID;
    NodeId firstChildId = invalidNodeID;
    NodeId lastChildId = invalidNodeID;
    NodeId nextSiblingId = invalidNodeID;
    uint16_t indent = 0;
    uint16_t numChildren = 0U;

    Node() = default;

    explicit Node(uint32_t indent) : indent(indent) {
    }
};
static_assert(sizeof(Node) == 32, "");
using NodesCache = StackVec<Node, 512>;

constexpr bool isUnused(Line::LineType lineType) {
    switch (lineType) {
    default:
        return false;
    case Line::LineType::empty:
        return true;
    case Line::LineType::comment:
        return true;
    case Line::LineType::fileSection:
        return true;
    }
}

bool buildTree(const LinesCache &lines, const TokensCache &tokens, NodesCache &outNodes, std::string &outErrReason, std::string &outWarning);

inline const Node *findChildByKey(const Node &parent, const NodesCache &allNodes, const TokensCache &allTokens, const ConstStringRef key) {
    auto childId = parent.firstChildId;
    while (invalidNodeID != childId) {
        if ((invalidTokenId != allNodes[childId].key) && (key == allTokens[allNodes[childId].key])) {
            break;
        }
        childId = allNodes[childId].nextSiblingId;
    }

    return (invalidNodeID != childId) ? &allNodes[childId] : nullptr;
}

inline const Node *getFirstChild(const Node &parent, const NodesCache &allNodes) {
    auto childId = parent.firstChildId;
    if (invalidNodeID == childId) {
        return nullptr;
    }

    return &allNodes[childId];
}

inline const Node *getLastChild(const Node &parent, const NodesCache &allNodes) {
    auto childId = parent.lastChildId;
    if (invalidNodeID == childId) {
        return nullptr;
    }

    return &allNodes[childId];
}

struct ConstSiblingsFwdIterator {
    // iterator traits
    using difference_type = long;
    using value_type = long;
    using pointer = const long *;
    using reference = const long &;
    using iterator_category = std::forward_iterator_tag;

    ConstSiblingsFwdIterator(NodeId currId, const NodesCache *allNodes)
        : allNodes(allNodes), currId(currId) {
    }

    ConstSiblingsFwdIterator(const ConstSiblingsFwdIterator &rhs)
        : allNodes(rhs.allNodes), currId(rhs.currId) {
    }

    ConstSiblingsFwdIterator &operator=(const ConstSiblingsFwdIterator &rhs) {
        allNodes = rhs.allNodes;
        currId = rhs.currId;
        return *this;
    }

    bool operator==(const ConstSiblingsFwdIterator &rhs) const {
        return (allNodes == rhs.allNodes) & (currId == rhs.currId);
    }

    bool operator!=(const ConstSiblingsFwdIterator &rhs) const {
        return false == (*this == rhs);
    }

    const Node &operator*() {
        return (*allNodes)[currId];
    }

    const Node *operator->() {
        return &(*allNodes)[currId];
    }

    ConstSiblingsFwdIterator &operator++() {
        if (invalidNodeID != currId) {
            currId = (*allNodes)[currId].nextSiblingId;
        }
        return *this;
    }

    ConstSiblingsFwdIterator operator++(int) {
        auto nextId = currId;
        if (invalidNodeID != currId) {
            nextId = (*allNodes)[currId].nextSiblingId;
        }
        auto prevId = currId;
        currId = nextId;
        return ConstSiblingsFwdIterator(prevId, allNodes);
    }

  protected:
    const NodesCache *allNodes = nullptr;
    NodeId currId = invalidNodeID;
};

struct ConstChildrenRange {
    ConstChildrenRange(const Node &first, const NodesCache &allNodes)
        : allNodes(allNodes), firstId(first.id) {
    }

    ConstChildrenRange(const NodeId firstId, const NodesCache &allNodes)
        : allNodes(allNodes), firstId(firstId) {
    }

    ConstSiblingsFwdIterator begin() const {
        return ConstSiblingsFwdIterator(firstId, &allNodes);
    }

    ConstSiblingsFwdIterator end() const {
        return ConstSiblingsFwdIterator(invalidNodeID, &allNodes);
    }

  protected:
    const NodesCache &allNodes;
    const NodeId firstId = invalidNodeID;
};

struct DebugNode {
    ~DebugNode() {
        for (auto c : children) {
            delete c;
        }
    }

    ConstStringRef key;
    std::vector<DebugNode *> children;
    ConstStringRef value;
    DebugNode *parent = nullptr;
    const Node *src = nullptr;
};

DebugNode *buildDebugNodes(NEO::Yaml::NodeId rootId, const NEO::Yaml::NodesCache &nodes, const NEO::Yaml::TokensCache &tokens);

struct YamlParser {
    YamlParser() {
    }

    bool parse(const ConstStringRef text, std::string &outErrReason, std::string &outWarning) {
        auto success = NEO::Yaml::tokenize(text, lines, tokens, outErrReason, outWarning);
        success = success && NEO::Yaml::buildTree(lines, tokens, nodes, outErrReason, outWarning);
        if (false == success) {
            nodes.clear();
        }
        return success;
    }

    bool empty() const {
        return (0U == nodes.size());
    }

    const Node *getRoot() const {
        return &nodes[0];
    }

    ConstStringRef readKey(const Node &node) const {
        return (invalidTokenId != node.key) ? tokens[node.key].cstrref() : "";
    }

    ConstStringRef readValue(const Node &node) const {
        return (invalidTokenId != node.value) ? tokens[node.value].cstrref() : "";
    }

    const Token *getValueToken(const Node &node) const {
        return (invalidTokenId != node.value) ? &tokens[node.value] : nullptr;
    }

    template <typename T>
    bool readValueChecked(const Node &node, T &outValue) const;

    ConstStringRef readValueNoQuotes(const Node &node) const {
        if (invalidTokenId == node.value) {
            return "";
        }
        auto &tok = tokens[node.value];
        if (Token::Type::literalString != tok.traits.type) {
            return tok.cstrref();
        }
        if ((tok.traits.character0 != '\'') && (tok.traits.character0 != '\"')) {
            return tok.cstrref();
        }
        return ConstStringRef(tok.pos + 1, tok.len - 2);
    }

    ConstChildrenRange createChildrenRange(const Node &parent) const {
        if (0 == parent.numChildren) {
            return ConstChildrenRange(invalidNodeID, nodes);
        }
        return ConstChildrenRange(nodes[parent.firstChildId], nodes);
    }

    const Node *findNodeWithKeyDfs(const ConstStringRef key) const {
        for (auto &node : nodes) {
            if (readKey(node) == key) {
                return &node;
            }
        }
        return nullptr;
    }

    const Node *getChild(const Node &parent, const ConstStringRef key) const {
        return findChildByKey(parent, nodes, tokens, key);
    }

    DebugNode *buildDebugNodes(const Node &parent) const;
    DebugNode *buildDebugNodes() const;

  protected:
    TokensCache tokens;
    LinesCache lines;
    NodesCache nodes;
};

template <>
inline bool YamlParser::readValueChecked<int64_t>(const Node &node, int64_t &outValue) const {
    if (invalidTokenId == node.value) {
        return false;
    }
    const auto &token = tokens[node.value];
    if (Token::Type::literalNumber != token.traits.type) {
        return false;
    }
    StackVec<char, 96> nullTerminated{token.pos, token.pos + token.len};
    nullTerminated.push_back('\0');

    char *endPtr;
    if (token.len > 2 && nullTerminated[0] == '0' && (nullTerminated[1] == 'x' || nullTerminated[1] == 'X')) {
        outValue = std::strtoll(nullTerminated.begin(), &endPtr, 16);
    } else {
        outValue = std::strtoll(nullTerminated.begin(), &endPtr, 10);
    }

    return (*endPtr == '\0');
}

template <>
inline bool YamlParser::readValueChecked<int32_t>(const Node &node, int32_t &outValue) const {
    int64_t int64V = 0U;
    bool validValue = readValueChecked<int64_t>(node, int64V);
    validValue &= int64V <= std::numeric_limits<int32_t>::max();
    validValue &= int64V >= std::numeric_limits<int32_t>::min();
    outValue = static_cast<int32_t>(int64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<int16_t>(const Node &node, int16_t &outValue) const {
    int64_t int64V = 0U;
    bool validValue = readValueChecked<int64_t>(node, int64V);
    validValue &= int64V <= std::numeric_limits<int16_t>::max();
    validValue &= int64V >= std::numeric_limits<int16_t>::min();
    outValue = static_cast<int16_t>(int64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<int8_t>(const Node &node, int8_t &outValue) const {
    int64_t int64V = 0U;
    bool validValue = readValueChecked<int64_t>(node, int64V);
    validValue &= int64V <= std::numeric_limits<int8_t>::max();
    validValue &= int64V >= std::numeric_limits<int8_t>::min();
    outValue = static_cast<int8_t>(int64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<uint64_t>(const Node &node, uint64_t &outValue) const {
    int64_t int64V = 0U;
    bool validValue = readValueChecked<int64_t>(node, int64V);
    validValue &= int64V >= 0;
    outValue = static_cast<uint64_t>(int64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<uint32_t>(const Node &node, uint32_t &outValue) const {
    uint64_t uint64V = 0U;
    bool validValue = readValueChecked<uint64_t>(node, uint64V);
    validValue &= uint64V <= std::numeric_limits<uint32_t>::max();
    outValue = static_cast<uint32_t>(uint64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<uint16_t>(const Node &node, uint16_t &outValue) const {
    uint64_t uint64V = 0U;
    bool validValue = readValueChecked<uint64_t>(node, uint64V);
    validValue &= uint64V <= std::numeric_limits<uint16_t>::max();
    outValue = static_cast<uint16_t>(uint64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<uint8_t>(const Node &node, uint8_t &outValue) const {
    uint64_t uint64V = 0U;
    bool validValue = readValueChecked<uint64_t>(node, uint64V);
    validValue &= uint64V <= std::numeric_limits<uint8_t>::max();
    outValue = static_cast<uint8_t>(uint64V);
    return validValue;
}

template <>
inline bool YamlParser::readValueChecked<bool>(const Node &node, bool &outValue) const {
    if (invalidTokenId == node.value) {
        return false;
    }
    const auto &token = tokens[node.value];
    if (Token::Type::literalString != token.traits.type) {
        return false;
    }

    // valid values : y/n yes/no true/false on/off (case insensitive)
    if (token.len > 5) {
        return false;
    }

    switch (token.traits.character0) {
    default:
        return false;
    case 'y':
    case 'Y': {
        outValue = true;
        switch (token.len) {
        default:
            return false;
        case 1:
            return true;
        case 3:
            return equalsCaseInsensitive(ConstStringRef("es"), ConstStringRef(token.cstrref().begin() + 1, 2));
        }
        break;
    }
    case 'n':
    case 'N': {
        outValue = false;
        switch (token.len) {
        default:
            return false;
        case 1:
            return true;
        case 2:
            return ((token.cstrref()[1] == 'o') || (token.cstrref()[1] == 'O'));
        }
        break;
    }
    case 't':
    case 'T': {
        outValue = true;
        if (token.len != 4) {
            return false;
        }
        return equalsCaseInsensitive(ConstStringRef("rue"), ConstStringRef(token.cstrref().begin() + 1, 3));
    }
    case 'f':
    case 'F': {
        outValue = false;
        if (token.len != 5) {
            return false;
        }
        return equalsCaseInsensitive(ConstStringRef("alse"), ConstStringRef(token.cstrref().begin() + 1, 4));
    }
    case 'o':
    case 'O': {
        switch (token.len) {
        default:
            return false;
        case 2:
            outValue = true;
            return ((token.cstrref()[1] == 'n') || (token.cstrref()[1] == 'N'));
        case 3:
            outValue = false;
            return equalsCaseInsensitive(ConstStringRef("ff"), ConstStringRef(token.cstrref().begin() + 1, 2));
        }
        break;
    }
    }

    return true;
}

template <>
inline bool YamlParser::readValueChecked<std::string>(const Node &node, std::string &outValue) const {
    const auto &token = tokens[node.value];
    if (Token::Type::literalString != token.traits.type) {
        return false;
    }
    outValue.assign(token.pos, token.len);
    return true;
}
} // namespace Yaml

} // namespace NEO
