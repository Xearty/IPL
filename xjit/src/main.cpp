#include "xjit/jit.h"
#include <Lexer.h>
#include <Parser.h>
#include <ASTPrinter.h>

#include <fstream>
#include <iostream>
#include <iomanip>

int main()
{
	X64Generator x64Generator;
	// const char* source = "function hello(a, b, c, d) { if (a > b) { return c; } else { return d; } }";
	// const char* source = "function hello(arg1, arg2, arg3, arg4, arg5, arg6, arg7) { var x = 5; return x + arg1 + 2 * arg2 + 3 * arg3 + 4 * arg4 + arg5 + arg6 + arg7; }";
	// const char* source = "function hello() { if (false) { return 100; } else { return 200; } }";
	
	// crashes on destruction
	const char* source = "function hello() { var x = 1; x = 2; return x; }";

	// const char* source = "function hello() { var x = 1; x = 2; x = 3; }";

	const auto& tokens = Tokenize(source).tokens;
	const auto AST = Parse(tokens);
	auto& expr = static_cast<TopStatements*>(AST.get())->GetValues()[0];

	PrintAST(expr, std::cout);

	const auto executable_memory = x64Generator.CompileFunction(expr.get());
	
	std::ofstream out("output.txt", std::ios::binary);

	for (const auto& byte : x64Generator.GetMachineCode())
	{
		out << std::hex << std::setw(2) << std::setfill('0') << (int)byte << ' ';
	}
	out.close();

	using Function = double(*)(double, double, double, double);
	Function func = (Function)executable_memory;
	auto result = func(2, 1, 100, 200);
	std::cout << "\nResult: " << result << std::endl;
}