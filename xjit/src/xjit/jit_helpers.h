#pragma once

#include "jit.h"

#define DOUBLE_ONE 0x3ff0000000000000

namespace
{
inline uint32_t GetDisplacement(int reg)
{
    return -(reg * (int)sizeof(double));
}

inline uint32_t CalculateRelative32BitOffset(uintptr_t addr1, uintptr_t addr2)
{
    return (uint32_t)(addr2 - addr1);
}

inline bool OperationIsComparison(TokenType token)
{
    return token == TokenType::Less || token == TokenType::LessEqual
        || token == TokenType::Greater || token == TokenType::GreaterEqual
        || token == TokenType::EqualEqual || token == TokenType::StrictEqual
        || token == TokenType::BangEqual || token == TokenType::StrictNotEqual;
}

inline Byte ComparisonOperationToByte(TokenType token)
{
    switch (token)
    {
        case TokenType::Less: return 0x01;
        case TokenType::LessEqual: return 0x02;
        case TokenType::Greater: return 0x06;
        case TokenType::GreaterEqual: return 0x05;
        case TokenType::EqualEqual: case TokenType::StrictEqual:    return 0x00;
        case TokenType::BangEqual:  case TokenType::StrictNotEqual: return 0x04;
    }
    NOT_IMPLEMENTED;
    return 0;
}

inline bool OperationIsArithmetic(TokenType token)
{
    return token == TokenType::Plus || token == TokenType::Minus
        || token == TokenType::Star || token == TokenType::Division;
}

inline Byte ArithmeticOperationToByte(TokenType token)
{
    switch (token)
    {
        case TokenType::Plus: return 0x58;
        case TokenType::Minus: return 0x5C;
        case TokenType::Star: return 0x59;
        case TokenType::Division: return 0x5E;
    }
    NOT_IMPLEMENTED;
    return 0;
}

inline bool OperationIsLogical(TokenType token)
{
    return token == TokenType::LogicalAnd || token == TokenType::LogicalOr;
}

inline Byte LogicalOperationToByte(TokenType token)
{
    switch (token)
    {
        case TokenType::LogicalAnd: return 0xDB;
        case TokenType::LogicalOr: return 0xEB;
    }
    NOT_IMPLEMENTED;
    return 0;
}

}
