#pragma once

#include "jit.h"
#include <unordered_set>

namespace
{
std::unordered_set<double> literal_addresses;

inline uint32_t GetDisplacement(int reg)
{
    return -(reg * (int)sizeof(double));
}

inline uint32_t CalculateRelative32BitOffset(uintptr_t addr1, uintptr_t addr2)
{
    return (uint32_t)(addr2 - addr1);
}
}
