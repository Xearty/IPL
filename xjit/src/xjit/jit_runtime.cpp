#include "jit.h"
#include "jit_helpers.h"
#include <iostream>

std::ostream* output_stream = &std::cout;

namespace
{
void Print(double arg)
{
	*output_stream << arg << std::endl;
}
}

using RuntimeFunction = void(*)(double);

IPLUnorderedMap<IPLString, RuntimeFunction> runtime;

bool IdentifierIsFunction(const IPLString& name)
{
	return runtime.find(name) != runtime.end();
}

void X64Generator::InitializeRuntime()
{
	runtime.emplace("print", Print);
}

void X64Generator::InvokeFunction(BinaryExpression* e)
{
	const auto left = e->GetLeft().get();
	const auto right = e->GetRight().get();
	// ewww
	const auto identifier = dynamic_cast<IdentifierExpression*>(dynamic_cast<Call*>(left)->GetObjectOrCall().get());

	if (!identifier || !IdentifierIsFunction(identifier->GetName()))
	{
		std::cerr << "Expected a function identifier to the left of |" << std::endl;
		return;
	}

	m_RegisterStack.push(GetRegisterForExpression(right));
	right->Accept(*this);

	// movsd xmm0, QWORD PTR [rbp-0x11223344]
	PushBytes(0xF2, 0x0F, 0x10, 0x85);
	Push4Bytes(GetDisplacement(m_RegisterStack.top()));
	m_RegisterStack.pop();

	RuntimeFunction function_address = runtime.at(identifier->GetName());

	// movabs rax, 0x1122334455667788
	PushBytes(0x48, 0xB8);
	Push8Bytes((uint64_t)function_address);

	// call rax
	PushBytes(0xFF, 0xD0);
}
