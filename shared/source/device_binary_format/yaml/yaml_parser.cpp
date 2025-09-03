/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/yaml/yaml_parser.h"

namespace NEO {

namespace Yaml {

std::string constructYamlError(size_t lineNumber, const char *lineBeg, const char *parsePos, const char *reason) {
    auto ret = "NEO::Yaml : Could not parse line : [" + std::to_string(lineNumber) + "] : [" + ConstStringRef(lineBeg, parsePos - lineBeg + 1).str() + "] <-- parser position on error";
    if (nullptr != reason) {
        ret += ". Reason : ";
        ret.append(reason);
    }
    ret += "\n";
    return ret;
}

inline Node &addNode(NodesCache &outNodes, Node &parent) {
    UNRECOVERABLE_IF(outNodes.size() >= outNodes.capacity()); // resize must not grow
    parent.firstChildId = static_cast<NodeId>(outNodes.size());
    parent.lastChildId = static_cast<NodeId>(outNodes.size());
    outNodes.push_back(Node());
    auto &curr = *outNodes.rbegin();
    curr.id = parent.lastChildId;
    curr.parentId = parent.id;
    ++parent.numChildren;
    return curr;
}

inline Node &addNode(NodesCache &outNodes, Node &prevSibling, Node &parent) {
    UNRECOVERABLE_IF(outNodes.size() >= outNodes.capacity()); // resize must not grow
    prevSibling.nextSiblingId = static_cast<NodeId>(outNodes.size());
    outNodes.push_back(Node());
    auto &curr = *outNodes.rbegin();
    curr.id = prevSibling.nextSiblingId;
    curr.parentId = parent.id;
    parent.lastChildId = curr.id;
    ++parent.numChildren;
    return curr;
}

struct TokenizerContext {
    TokenizerContext(ConstStringRef text)
        : pos(text.begin()),
          end(text.end()),
          lineBeginPos(text.begin()) {
        lineTraits.reset();
    }

    const char *pos = nullptr;
    const char *const end = nullptr;

    uint32_t lineIndent = 0U;
    TokenId lineBegin = 0U;
    const char *lineBeginPos = nullptr;
    bool isParsingIdent = false;
    Line::LineTraits lineTraits;
};

bool tokenizeEndLine(ConstStringRef text, LinesCache &outLines, TokensCache &outTokens, std::string &outErrReason, std::string &outWarning, TokenizerContext &context) {
    TokenId lineEnd = static_cast<uint32_t>(outTokens.size());
    outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
    auto lineBegToken = outTokens[context.lineBegin];
    Line::LineType lineType = Line::LineType::empty;
    if (lineEnd != context.lineBegin) {
        switch (lineBegToken.traits.type) {
        default:
            outErrReason = constructYamlError(outLines.size(), lineBegToken.pos, context.pos, "Internal error - undefined line type");
            return false;
        case Token::singleCharacter:
            switch (lineBegToken.traits.character0) {
            default:
                outErrReason = constructYamlError(outLines.size(), lineBegToken.pos, context.pos, (std::string("Unhandled keyword character : ") + lineBegToken.traits.character0).c_str());
                return false;
            case '#':
                lineType = Line::LineType::comment;
                break;
            case '-':
                lineType = Line::LineType::listEntry;
                break;
            }
            break;
        case Token::identifier:
            lineType = Line::LineType::dictionaryEntry;
            break;
        case Token::fileSectionBeg:
            lineType = Line::LineType::fileSection;
            break;
        case Token::fileSectionEnd:
            lineType = Line::LineType::fileSection;
            break;
        }
    }
    outLines.push_back(Line{lineType, static_cast<uint16_t>(context.lineIndent), context.lineBegin, lineEnd, context.lineTraits});
    ++context.pos;

    context.lineIndent = 0U;
    context.lineBegin = static_cast<uint32_t>(outTokens.size());
    context.lineBeginPos = context.pos;
    context.isParsingIdent = true;
    context.lineTraits.reset();
    return true;
}

bool isValidInlineCollectionFormat(const char *context, const char *contextEnd) {
    auto consumeAlphaNum = [](const char *&text) {
        while (isAlphaNumeric(*text)) {
            text++;
        }
    };

    bool endNum = false;
    bool endCollection = false;
    context++; // skip '['
    while (context < contextEnd && *context != '\n') {
        if (isWhitespace(*context)) {
            context++;
        } else if (false == endNum) {
            if (*context == ']') {
                context++;
                endCollection = true;
            } else if (isAlphaNumeric(*context)) {
                consumeAlphaNum(context);
                endNum = true;
            } else {
                return false;
            }
        } else if (false == endCollection) {
            if (*context == ',') {
                context++;
                endNum = false;
            } else if (*context == ']') {
                context++;
                endCollection = true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    return endCollection;
}

bool tokenize(ConstStringRef text, LinesCache &outLines, TokensCache &outTokens, std::string &outErrReason, std::string &outWarning) {
    if (text.empty()) {
        outWarning.append("NEO::Yaml : input text is empty\n");
        return true;
    }

    TokenizerContext context{text};
    context.isParsingIdent = true;

    while (context.pos < context.end) {
        reserveBasedOnEstimates(outTokens, text.begin(), text.end(), context.pos);
        switch (context.pos[0]) {
        case ' ':
            context.lineIndent += context.isParsingIdent ? 1 : 0;
            ++context.pos;
            break;
        case '\t':
            if (context.isParsingIdent) {
                context.lineIndent += 4U;
                outWarning.append("NEO::Yaml : Tabs used as indent at line : " + std::to_string(outLines.size()) + "\n");
            }
            ++context.pos;
            break;
        case '\r':
        case '\0':
            ++context.pos;
            break;
        case '#': {
            context.isParsingIdent = false;
            outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
            auto commentIt = context.pos + 1;
            while (commentIt < context.end) {
                if ('\n' == commentIt[0]) {
                    break;
                }
                ++commentIt;
            }
            if (context.pos + 1 != commentIt) {
                outTokens.push_back(Token(ConstStringRef(context.pos + 1, commentIt - (context.pos + 1)), Token::comment));
            }
            context.pos = commentIt;
            break;
        }
        case '\n': {
            reserveBasedOnEstimates(outLines, text.begin(), text.end(), context.pos);
            if (false == tokenizeEndLine(text, outLines, outTokens, outErrReason, outWarning, context)) {
                return false;
            }
        } break;
        case '\"':
        case '\'': {
            context.isParsingIdent = false;
            auto parseTokEnd = consumeStringLiteral(text, context.pos);
            if (parseTokEnd == context.pos) {
                outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, "Unterminated string");
                return false;
            }
            outTokens.push_back(Token(ConstStringRef(context.pos, parseTokEnd - context.pos), Token::literalString));
            context.pos = parseTokEnd;
            break;
        }
        case '-': {
            ConstStringRef fileSectionMarker("---");
            if ((context.isParsingIdent) && isMatched(text, context.pos, fileSectionMarker)) {
                outTokens.push_back(Token(ConstStringRef(context.pos, fileSectionMarker.size()), Token::fileSectionBeg));
                context.pos += fileSectionMarker.size();
            } else {
                auto tokEnd = consumeNumberOrSign(text, context.pos);
                if (tokEnd > context.pos + 1) {
                    outTokens.push_back(Token(ConstStringRef(context.pos, tokEnd - context.pos), Token::literalNumber));
                } else {
                    outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
                }

                context.pos = tokEnd;
            }
            context.isParsingIdent = false;
            break;
        }
        case '.': {
            ConstStringRef fileSectionMarker("...");
            if ((context.isParsingIdent) && isMatched(text, context.pos, fileSectionMarker)) {
                outTokens.push_back(Token(ConstStringRef(context.pos, fileSectionMarker.size()), Token::fileSectionEnd));
                context.pos += fileSectionMarker.size();
            } else {
                outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
                ++context.pos;
            }
            context.isParsingIdent = false;
            break;
        }
        case '[':
            if (false == isValidInlineCollectionFormat(context.pos, text.end())) {
                outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, inlineCollectionYamlErrorMsg.data());
                return false;
            }
            context.lineTraits.hasInlineDataMarkers = true;
            outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::collectionBeg));
            ++context.pos;
            break;
        case ']':
            if (false == context.lineTraits.hasInlineDataMarkers) {
                outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, inlineCollectionYamlErrorMsg.data());
                return false;
            }
            outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::collectionEnd));
            ++context.pos;
            break;
        case ',':
            if (false == context.lineTraits.hasInlineDataMarkers) {
                outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, inlineCollectionYamlErrorMsg.data());
                return false;
            }
            outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
            ++context.pos;
            break;
        case '{':
        case '}':
            outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, "NEO::Yaml : Inline dictionaries are not supported");
            return false;
        case ':':
            context.lineTraits.hasDictionaryEntry = true;
            context.isParsingIdent = false;
            outTokens.push_back(Token(ConstStringRef(context.pos, 1), Token::singleCharacter));
            ++context.pos;
            break;
        default: {
            context.isParsingIdent = false;
            auto tokEnd = consumeNameIdentifier(text, context.pos);
            if (tokEnd != context.pos) {
                auto tokenData = ConstStringRef(context.pos, tokEnd - context.pos);
                tokenData = tokenData.trimEnd(isWhitespace);
                if (context.lineTraits.hasDictionaryEntry) {
                    outTokens.push_back(Token(tokenData, Token::literalString));
                } else {
                    outTokens.push_back(Token(tokenData, Token::identifier));
                }
            } else {
                tokEnd = consumeNumberOrSign(text, context.pos);
                if (tokEnd > context.pos) {
                    outTokens.push_back(Token(ConstStringRef(context.pos, tokEnd - context.pos), Token::literalNumber));
                } else {
                    outErrReason = constructYamlError(outLines.size(), context.lineBeginPos, context.pos, "Invalid numeric literal");
                    return false;
                }
            }

            context.pos = tokEnd;
            break;
        }
        }
    }

    if (outTokens.empty()) {
        outWarning.append("NEO::Yaml : text tokenized to 0 tokens\n");
    } else {
        if ('\n' != *outTokens.rbegin()) {
            outWarning.append("NEO::Yaml : text does not end with newline\n");
            tokenizeEndLine(text, outLines, outTokens, outErrReason, outWarning, context);
        }
    }
    return true;
}

void finalizeNode(NodeId nodeId, const TokensCache &tokens, NodesCache &outNodes, std::string &outErrReason, std::string &outWarning) {
    auto &node = outNodes[nodeId];
    if (invalidTokenId != node.key) {
        return;
    }
    if (invalidTokenId == node.value) {
        return;
    }
    auto valueTokenIt = node.value + 1;
    auto colon = invalidTokenId;
    while ('\n' != tokens[valueTokenIt]) {
        if (':' == tokens[valueTokenIt]) {
            colon = valueTokenIt;
        }
        ++valueTokenIt;
    }
    UNRECOVERABLE_IF((colon == invalidTokenId) || (colon + 1 == valueTokenIt));
    UNRECOVERABLE_IF(invalidNodeID == node.lastChildId)
    outNodes[node.lastChildId].nextSiblingId = static_cast<NodeId>(outNodes.size());

    outNodes.push_back(Node());
    auto &newNode = *outNodes.rbegin();
    newNode.id = static_cast<NodeId>(outNodes.size() - 1);
    newNode.parentId = nodeId;
    node.lastChildId = outNodes.rbegin()->id;
    newNode.key = node.value;
    newNode.value = colon + 1;

    node.value = invalidTokenId;
    ++node.numChildren;
}

bool isEmptyVector(const Token &token, size_t lineId, std::string &outError) {
    if (isVectorDataType(token)) {
        if (!allowEmptyVectorDataType(token)) {
            outError = constructYamlError(lineId, token.pos, token.pos + token.len, "Vector data type expects to have at least one value starting with -");
            return false;
        }
    }
    return true;
}

bool buildTree(const LinesCache &lines, const TokensCache &tokens, NodesCache &outNodes, std::string &outErrReason, std::string &outWarning) {
    StackVec<NodeId, 64> nesting;
    size_t lineId = 0U;
    size_t lastUsedLine = 0u;
    outNodes.push_back(Node());
    outNodes.rbegin()->id = 0U;
    outNodes.rbegin()->firstChildId = 1U;
    outNodes.rbegin()->lastChildId = 1U;
    nesting.resize(1); // root
    while (lineId < lines.size()) {
        if (isUnused(lines[lineId].lineType)) {
            ++lineId;
            continue;
        }
        auto currLineIndent = lines[lineId].indent;
        if (currLineIndent == outNodes.rbegin()->indent) {
            if (lineId > 0u && false == isEmptyVector(tokens[lines[lastUsedLine].first], lastUsedLine, outErrReason)) {
                return false;
            }
            reserveBasedOnEstimates(outNodes, static_cast<size_t>(0U), lines.size(), lineId);
            auto &prev = *outNodes.rbegin();
            auto &parent = outNodes[*nesting.rbegin()];
            auto &curr = addNode(outNodes, prev, parent);
            curr.indent = currLineIndent;
        } else if (currLineIndent > outNodes.rbegin()->indent) {
            reserveBasedOnEstimates(outNodes, static_cast<size_t>(0U), lines.size(), lineId);
            auto &parent = *outNodes.rbegin();
            auto &curr = addNode(outNodes, parent);
            curr.indent = currLineIndent;
            nesting.push_back(parent.id);
        } else {
            while (currLineIndent < outNodes[*nesting.rbegin()].indent) {
                reserveBasedOnEstimates(outNodes, static_cast<size_t>(0U), lines.size(), lineId);
                finalizeNode(*nesting.rbegin(), tokens, outNodes, outErrReason, outWarning);
                UNRECOVERABLE_IF(nesting.empty());
                nesting.pop_back();
            }
            bool hasInvalidIndent = (currLineIndent != outNodes[*nesting.rbegin()].indent);
            if (hasInvalidIndent) {
                outErrReason = constructYamlError(lineId, tokens[lines[lineId].first].pos, tokens[lines[lineId].first].pos + 1, "Invalid indentation");
                return false;
            } else {
                reserveBasedOnEstimates(outNodes, static_cast<size_t>(0U), lines.size(), lineId);
                auto &prev = outNodes[*nesting.rbegin()];
                auto &parent = outNodes[prev.parentId];
                auto &curr = addNode(outNodes, prev, parent);
                curr.indent = currLineIndent;
            }
        }

        if (Line::LineType::dictionaryEntry == lines[lineId].lineType) {
            auto numTokensInLine = lines[lineId].last - lines[lineId].first + 1;
            outNodes.rbegin()->key = lines[lineId].first;
            UNRECOVERABLE_IF(numTokensInLine < 3); // at least key, : and \n

            if (lines[lineId].traits.hasInlineDataMarkers) {
                auto collectionBeg = lines[lineId].first + 2;
                auto collectionEnd = lines[lineId].last - 1;
                UNRECOVERABLE_IF(tokens[collectionBeg].traits.type != Token::Type::collectionBeg || tokens[collectionEnd].traits.type != Token::Type::collectionEnd);

                if (collectionEnd - collectionBeg > 1) {
                    auto parentNodeId = outNodes.size() - 1;
                    auto previousSiblingId = std::numeric_limits<size_t>::max();

                    for (auto currTokenId = collectionBeg + 1; currTokenId < collectionEnd; currTokenId += 2) {
                        auto tokenType = tokens[currTokenId].traits.type;
                        UNRECOVERABLE_IF(tokenType != Token::Type::literalNumber && tokenType != Token::Type::literalString);
                        reserveBasedOnEstimates(outNodes, static_cast<size_t>(0U), lines.size(), lineId);

                        auto &parentNode = outNodes[parentNodeId];
                        if (previousSiblingId == std::numeric_limits<size_t>::max()) {
                            addNode(outNodes, parentNode);
                        } else {
                            auto &previousSibling = outNodes[previousSiblingId];
                            addNode(outNodes, previousSibling, parentNode);
                        }
                        previousSiblingId = outNodes.size() - 1;
                        outNodes[previousSiblingId].indent = currLineIndent + 1;
                        outNodes[previousSiblingId].value = currTokenId;
                    }
                    nesting.push_back(static_cast<NodeId>(parentNodeId));
                }
            } else if (('#' != tokens[lines[lineId].first + 2]) && ('\n' != tokens[lines[lineId].first + 2])) {
                outNodes.rbegin()->value = lines[lineId].first + 2;
            }
        } else {
            auto numTokensInLine = lines[lineId].last - lines[lineId].first + 1;
            (void)numTokensInLine;
            UNRECOVERABLE_IF(numTokensInLine < 2); // at least : - and \n
            UNRECOVERABLE_IF(Line::LineType::listEntry != lines[lineId].lineType);
            UNRECOVERABLE_IF('-' != tokens[lines[lineId].first]);
            if (('#' != tokens[lines[lineId].first + 1]) && ('\n' != tokens[lines[lineId].first + 1])) {
                outNodes.rbegin()->value = lines[lineId].first + 1;
            }
        }
        lastUsedLine = lineId;
        ++lineId;
    }
    outNodes.reserve(outNodes.size() + nesting.size());
    while (false == nesting.empty()) {
        finalizeNode(*nesting.rbegin(), tokens, outNodes, outErrReason, outWarning);
        nesting.pop_back();
    }
    if (1U == outNodes.size()) {
        outWarning.append("NEO::Yaml : Text has no data\n");
        outNodes.clear();
        return true;
    }
    return true;
}

DebugNode *buildDebugNodes(NEO::Yaml::NodeId rootId, const NEO::Yaml::NodesCache &nodes, const NEO::Yaml::TokensCache &tokens) {
    DebugNode *curr = new DebugNode;
    auto &src = nodes[rootId];
    curr->src = &src;
    if (src.key != NEO::Yaml::invalidTokenId) {
        curr->key = tokens[src.key].cstrref();
    }
    if (src.value != NEO::Yaml::invalidTokenId) {
        curr->value = tokens[src.value].cstrref();
    }

    auto childId = src.firstChildId;
    while (NEO::Yaml::invalidNodeID != childId) {
        curr->children.push_back(buildDebugNodes(childId, nodes, tokens));
        (*curr->children.rbegin())->parent = curr;
        childId = nodes[childId].nextSiblingId;
    }
    return curr;
}

DebugNode *YamlParser::buildDebugNodes(const Node &parent) const {
    return NEO::Yaml::buildDebugNodes(parent.id, nodes, tokens);
}

DebugNode *YamlParser::buildDebugNodes() const {
    return (false == empty()) ? NEO::Yaml::buildDebugNodes(0U, nodes, tokens) : nullptr;
}

} // namespace Yaml

} // namespace NEO
