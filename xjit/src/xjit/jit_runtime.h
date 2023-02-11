#pragma once

#include <CommonTypes.h>
#include <iostream>

extern IPLUnorderedMap<IPLString, void(*)(double)> runtime;
extern std::ostream* output_stream;

bool IdentifierIsFunction(const IPLString& name);
