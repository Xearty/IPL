#include <gtest/gtest.h>
#include <Lexer.h>
#include <Parser.h>
#include <xjit/jit.h>
#include <xjit/jit_runtime.h>

#include <fstream>

struct JitTest : testing::Test
{
	const Byte* Compile(const char* program)
	{
		auto tokens = Tokenize(program).tokens;
		auto expr = Parse(tokens);

		const auto top_statements = static_cast<TopStatements*>(expr.get());
		const auto function = top_statements->GetValues()[0].get();

		output_stream = &output;

		return generator.CompileFunction(function);
	}

	// generator needs to stay alive for the literals to be accessible
	X64Generator generator;
	std::ostringstream output;
};

TEST_F(JitTest, EmptyFunction)
{
	const auto* executable_code = Compile("function func() {}");
	auto function = (double(*)())executable_code;
	function();
	ASSERT_EQ(output.str(), "");
}

TEST_F(JitTest, ReturnNumberLiteral)
{
	const auto* executable_code = Compile("function func() { return 5; }");
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 5.0);
}

TEST_F(JitTest, ReturnVariable)
{
	const auto* executable_code = Compile(
		" function func() { "
		"     var x = 100;  "
		"     return x;     "
		" }                 "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 100.0);
}

TEST_F(JitTest, ReturnArgument)
{
	const auto* executable_code = Compile(
		" function func(arg) { "
		"     return arg;"
        " }"
	);
	using Function = double(*)(double);
	Function function = (Function)executable_code;
	double result = function(69.0);
	ASSERT_EQ(result, 69.0);
}

TEST_F(JitTest, ManyArguments)
{
	// Windows calling convention specifies that the first 4 arguments are in registers
	// and the rest are pushed onto the stack, this checks that functions work for more
	// than 4 arguments
	const auto* executable_code = Compile(
		" function func(a1, a2, a3, a4, a5, a6, a7, a8) {                 "
		"     return a1 + 2*a2 + 3*a3 + 4*a4 + 5*a5 + 6*a6 + 7*a7 + 8*a8; "
		" }                                                               "
	);
	using Function = double(*)(double, double, double, double, double, double, double, double);
	Function function = (Function)executable_code;
	double result = function(1, 2, 3, 4, 5, 6, 7, 8);
	ASSERT_EQ(result, 1 + 2*2 + 3*3 + 4*4 + 5*5 + 6*6 + 7*7 + 8*8);
}

TEST_F(JitTest, ArithmeticExpression)
{
	const auto* executable_code = Compile(
		" function func(x, y) {                                                         "
		"     return (x + 2 - y * 6 / 2 + (x + y) / 8) / 2 * 100 - 200 + x * y * y - 8; "
		" }                                                                             "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(5.0, 12.0);
	ASSERT_EQ(result, -831.75);
}

TEST_F(JitTest, ComparisonExpression)
{
	const auto* executable_code = Compile(
		" function func(x, y) {                                                           "
		"     return 8 * (x > y) + 15 * (x >= y) + (x == y) + 2 * (x < y) + 3 * (x <= y); "
		" }                                                                               "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(5.0, 5.0);
	ASSERT_EQ(result, 19.0);
}

TEST_F(JitTest, BooleanExpression)
{
	const auto* executable_code = Compile(
		" function func() {                                        "
		"     return true + true * 3 - false * 100 + true + false; "
		" }                                                        "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 5.0);
}

TEST_F(JitTest, NullIsZero)
{
	const auto* executable_code = Compile(
		" function func() {    "
		"     return null + 1; "
		" }                    "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 1.0);
}

TEST_F(JitTest, Assignment)
{
	const auto* executable_code = Compile(
		" function func() {                       "
		"     var variable = 5;                   "
		"     variable = 100;                     "
		"     variable = variable + 10;           "
		"     var secondVariable = variable + 15; "
		"     return secondVariable;              "
		" }                                       "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 125.0);
}

TEST_F(JitTest, IfTrue)
{
	const auto* executable_code = Compile(
		" function func() {  "
		"     if (true) {    "
		"         return 10; "
        "     }              "
		"     return 20;     "
		" }                  "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 10.0);
}

TEST_F(JitTest, IfFalse)
{
	const auto* executable_code = Compile(
		" function func() {  "
		"     if (false) {   "
		"         return 10; "
		"     }              "
		"     return 20;     "
		" }                  "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 20.0);
}

TEST_F(JitTest, IfElseTrue)
{
	const auto* executable_code = Compile(
		" function func() { "
		"     var x = 1;    "
		"     if (true) {   "
		"         x = 2;    "
		"     } else {      "
		"         x = 3;    "
		"     }             "
		"     return x;     "
		" }                 ");
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 2.0);
}

TEST_F(JitTest, IfElseFalse)
{
	const auto* executable_code = Compile(
		" function func() { "
		"     var x = 1;    "
		"     if (false) {  "
		"         x = 2;    "
		"     } else {      "
		"         x = 3;    "
		"     }             "
		"     return x;     "
		" }                 ");
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 3.0);
}

TEST_F(JitTest, IfElseConditionTrue)
{
	const auto* executable_code = Compile(
		" function func(a, b) { "
		"     var x = 1;        "
		"     if (a < b) {      "
		"         x = 2;        "
		"     } else {          "
		"         x = 3;        "
		"     }                 "
		"     return x;         "
		" }                     ");
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(1.0, 2.0);
	ASSERT_EQ(result, 2.0);
}

TEST_F(JitTest, IfElseConditionFalse)
{
	const auto* executable_code = Compile(
		" function func(a, b) { "
		"     var x = 1;        "
		"     if (a > b) {      "
		"         x = 2;        "
		"     } else {          "
		"         x = 3;        "
		"     }                 "
		"     return x;         "
		" }                     ");
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(1.0, 2.0);
	ASSERT_EQ(result, 3.0);
}

TEST_F(JitTest, RuntimePrint)
{
	const auto* executable_code = Compile(
		" function func(x) {             "
		"     print | 10;                "
		"     print | 20;                "
		"                                "
		"     var variable = 69;         "
		"     print | variable;          "
		"     print | variable + 100;    "
		"                                "
		"     print | x;                 "
		"     print | x + variable + 10; "
		"                                "
		"     return 0;                  "
		" }                              "
	);
	using Function = double(*)(double);
	Function function = (Function)executable_code;
	double result = function(20.0);
	ASSERT_EQ(result, 0.0);
	ASSERT_EQ(output.str(), "10\n20\n69\n169\n20\n99\n");
}

TEST_F(JitTest, WhileTrue)
{
	const auto* executable_code = Compile(
		" function func() {  "
		"     while (true) { "
		"         return 10; "
		"     }              "
		"     return 20;     "
		" }                  "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 10.0);
}

TEST_F(JitTest, WhileFalse)
{
	const auto* executable_code = Compile(
		" function func() {   "
		"     var x = 10;     "
		"     while (false) { "
		"         x = 20;     "
		"     }               "
		"     return x;       "
		" }                   "
	);
	using Function = double(*)();
	Function function = (Function)executable_code;
	double result = function();
	ASSERT_EQ(result, 10.0);
}

TEST_F(JitTest, WhileTrueCondition)
{
	const auto* executable_code = Compile(
		" function func(x, y) { "
		"     while (x == y) {  "
		"         return 10;    "
		"     }                 "
		"     return 20;        "
		" }                     "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(2.0, 2.0);
	ASSERT_EQ(result, 10.0);
}

TEST_F(JitTest, WhileFalseCondition)
{
	const auto* executable_code = Compile(
		" function func(x, y) { "
		"     while (x == y) {  "
		"         return 10;    "
		"     }                 "
		"     return 20;        "
		" }                     "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(1.0, 2.0);
	ASSERT_EQ(result, 20.0);
}

TEST_F(JitTest, WhileOneThroughTen)
{
	const auto* executable_code = Compile(
		" function func(low, up) { "
		"     while (low <= up) {  "
		"         print | low;     "
		"         low = low + 1;   "
		"     }                    "
		"     return 0;            "
		" }                        "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(1.0, 10.0);
	ASSERT_EQ(result, 0.0);
	ASSERT_EQ(output.str(), "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
}

TEST_F(JitTest, ForOneThroughTen)
{
	const auto* executable_code = Compile(
		" function func(low, up) {                    "
		"     for (var i = low; i <= up; i = i + 1) { "
		"         print | i;                          "
		"     }                                       "
		"     return 0;                               "
		" }                                           "
	);
	using Function = double(*)(double, double);
	Function function = (Function)executable_code;
	double result = function(1.0, 10.0);
	ASSERT_EQ(result, 0.0);
	ASSERT_EQ(output.str(), "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n");
}

TEST_F(JitTest, FibN)
{
	const auto* executable_code = Compile(
		" function fib(n) {                       "
		"     var prev = 0;                       "
		"     var current = 1;                    "
		"     for (var i = 0; i < n; i = i + 1) { "
		"         print | current;                "
		"                                         "
		"         var next = prev + current;      "
		"         prev = current;                 "
		"         current = next;                 "
		"     }                                   "
		"     return 0;                           "
		" }                                       "
	);
	using Function = double(*)(double);
	Function function = (Function)executable_code;
	double result = function(12);
	ASSERT_EQ(result, 0.0);
	ASSERT_EQ(output.str(), "1\n1\n2\n3\n5\n8\n13\n21\n34\n55\n89\n144\n");
}
