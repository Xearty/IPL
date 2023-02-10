#pragma once

#include <CommonTypes.h>

extern IPLUnorderedMap<IPLString, void(*)(double)> runtime;

bool IdentifierIsFunction(const IPLString& name);
