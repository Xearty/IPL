#include "jit.h"
#include "jit_helpers.h"

X64Generator::X64Generator()
{
    literals.reserve(1000);
}

const Byte* X64Generator::CompileFunction(Expression* e)
{
    next_register = 1;
    e->Accept(*this);
    return GetExecutableMemory();
}

void X64Generator::Visit(LiteralNumber* e)
{
    MovRegNumber(registers.top(), e->GetValue());
}

void X64Generator::Visit(LiteralBoolean* e)
{
    MovRegNumber(registers.top(), e->GetValue());
}

void X64Generator::Visit(IdentifierExpression* e)
{
    MovRegs(registers.top(), GetIdentifierRegister(e->GetName()));
}

void X64Generator::Visit(Call* e)
{
    e->GetObjectOrCall()->Accept(*this);
}

void X64Generator::Visit(BinaryExpression* e)
{
    registers.push(GetNewRegister());
    e->GetLeft()->Accept(*this);

    registers.push(GetNewRegister());
    e->GetRight()->Accept(*this);

    auto second_reg = registers.top();
    registers.pop();
    auto first_reg = registers.top();
    registers.pop();

    // movsd  xmm0, QWORD PTR[rbp - 0x8]
    PushBytes(0xF2, 0x0F, 0x10, 0x85);
    Push4Bytes(GetDisplacement(first_reg));

    // movsd  xmm1, QWORD PTR [rbp-0x10]
    PushBytes(0xF2, 0x0F, 0x10, 0x8D);
    Push4Bytes(GetDisplacement(second_reg));

    const TokenType op = e->GetOperator();

    if (OperationIsArithmetic(op))
    {
        PushBytes(0xF2, 0x0F, ArithmeticOperationToByte(op), 0xC1);
    }
    else if (OperationIsComparison(op))
    {
        // cmp*sd xmm0, xmm1
        PushBytes(0xF2, 0x0F, 0xC2, 0xC1, ComparisonOperationToByte(op));

        // So that we get 1 for true and 0 for false
        const int one = GetNewRegister();
        MovRegNumberRaw(one, DOUBLE_ONE);

        // movsd  xmm1, QWORD PTR [rbp-0x10]
        PushBytes(0xF2, 0x0F, 0x10, 0x8D);
        Push4Bytes(GetDisplacement(one));

        // pand  xmm0, xmm1
        PushBytes(0x66, 0x0F, 0xDB, 0xC1);
    }

    // movq    QWORD PTR [rbp+0x0], xmm0
    PushBytes(0x66, 0x0F, 0xD6, 0x85);
    Push4Bytes(GetDisplacement(registers.top()));
}

void X64Generator::Visit(UnaryExpression* e)
{
    switch (e->GetOperator())
    {
        // @TODO: don't emit jump if already at the end
        case TokenType::Return:
        {
            registers.push(GetNewRegister());
            e->GetExpr()->Accept(*this);

            // movsd xmm0, QWORD PTR [bsp-0x00]
            PushBytes(0xF2, 0x0F, 0x10, 0x85);
            Push4Bytes(GetDisplacement(registers.top()));

            registers.pop();

            // jmp to end of function
            return_fixup_offsets.push(executable_memory.size());
            PushBytes(0xE9);
            Push4Bytes(0x00);

            // need to jump and pop registers
        } break;
    }
}

void X64Generator::Visit(VariableDefinitionExpression* e)
{
    PushIdentifier(e->GetName());
    
    registers.push(GetNewRegister());
    e->GetValue()->Accept(*this);

    const auto variable_reg = GetIdentifierRegister(e->GetName());
    const auto value_reg = registers.top();
    MovRegs(variable_reg, value_reg);
    registers.pop();
}

void X64Generator::Visit(BlockStatement* e)
{
    for (auto& expr : e->GetValues())
    {
        expr->Accept(*this);
    }
}

void X64Generator::Visit(IfStatement* e)
{
    // check if condition is true
    registers.push(GetNewRegister());
    e->GetCondition()->Accept(*this);

    // pxor xmm0, xmm0
    PushBytes(0x66, 0x0F, 0xEF, 0xC0);

    // ucomisd xmm0, QWORD PTR [rbp-0x11223344]
    PushBytes(0x66, 0x0F, 0x2E, 0x85);
    Push4Bytes(GetDisplacement(registers.top()));

    // check je and jp (equality and parity)
    //jp
    jump_fixup_offsets.push(executable_memory.size());
    PushBytes(0x0F, 0x8A);
    Push4Bytes(0x00);

    // ucomisd xmm0, QWORD PTR [rbp-0x11223344]
    PushBytes(0x66, 0x0F, 0x2E, 0x85);
    Push4Bytes(GetDisplacement(registers.top()));

    registers.pop();

    // je
    jump_fixup_offsets.push(executable_memory.size());
    PushBytes(0x0F, 0x84);
    Push4Bytes(0x00);

    e->GetIfStatement()->Accept(*this);

    // else
    // fix jump offsets
    uintptr_t je_offset = jump_fixup_offsets.top();
    Replace32BitsAtOffset(je_offset + 2, CalculateRelative32BitOffset(je_offset + 6, executable_memory.size()));
    jump_fixup_offsets.pop();
    
    uintptr_t jp_offset = jump_fixup_offsets.top();
    Replace32BitsAtOffset(jp_offset + 2, CalculateRelative32BitOffset(jp_offset + 6, executable_memory.size()));
    jump_fixup_offsets.pop();

    e->GetElseStatement()->Accept(*this);

    // if true, execute the first block and jump at end

    // if false, jump to the else block if exists, if not, jump after the if
}

void X64Generator::Visit(ListExpression* e)
{
    for (auto& expr : e->GetValues())
    {
        expr->Accept(*this);
    }
}

void X64Generator::Visit(FunctionDeclaration* e)
{
    // push    rbp
    // mov     rbp, rsp
    PushBytes(0x55, 0x48, 0x89, 0xE5);

    // arguments (all doubles)
    const auto& args = e->GetArgumentsIdentifiers();

    for (int i = 0; i < args.size() && i < 4; ++i)
    {
        const auto& name = args[i];
        PushIdentifier(name);

        // 0x85 is for xmm0 and the rest are across 8 bytes
        Byte floating_point_register = (Byte)(0x85 + (i * 8));
        PushBytes(0x66, 0x0F, 0xD6, floating_point_register);
        Push4Bytes(GetDisplacement(GetIdentifierRegister(name)));
    }

    // the rest are pushed on the stack
    for (int i = 4; i < args.size(); ++i)
    {
        const auto& name = args[i];

        // this is probably not right but it works
        SetIdentifierRegister(name, -(i + 2));
    }

    e->GetBody()->Accept(*this);

    // fixup returns
    // probably better to ret on return and not have to fix up offsets
    while (!return_fixup_offsets.empty())
    {
        const uintptr_t offset = return_fixup_offsets.top();
        Replace32BitsAtOffset(offset + 1, CalculateRelative32BitOffset(offset + 5, executable_memory.size()));
        return_fixup_offsets.pop();
    }

    // pop  rbp
    // ret
    PushBytes(0x5D, 0xC3);
}

void X64Generator::Visit(TopStatements* e)
{
    for (auto& statement : e->GetValues())
    {
        statement->Accept(*this);
    }
}
