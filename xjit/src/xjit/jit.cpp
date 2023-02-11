#include "jit.h"
#include "jit_helpers.h"
#include "jit_runtime.h"

X64Generator::X64Generator()
{
    InitializeRuntime();
}

const Byte* X64Generator::CompileFunction(Expression* e)
{
    ResetState();
    e->Accept(*this);
    return GetExecutableMemory();
}

void X64Generator::Visit(LiteralUndefined* e)
{
    (void)e;
    MovRegNumber(m_RegisterStack.top(), 0);
}

void X64Generator::Visit(LiteralNull* e)
{
    (void)e;
    MovRegNumber(m_RegisterStack.top(), 0);
}

void X64Generator::Visit(LiteralNumber* e)
{
    MovRegNumber(m_RegisterStack.top(), e->GetValue());
}

void X64Generator::Visit(LiteralBoolean* e)
{
    MovRegNumber(m_RegisterStack.top(), e->GetValue());
}

void X64Generator::Visit(IdentifierExpression* e)
{
    MovRegs(m_RegisterStack.top(), GetIdentifierRegister(e->GetName()));
}

void X64Generator::Visit(Call* e)
{
    e->GetObjectOrCall()->Accept(*this);
}

void X64Generator::Visit(BinaryExpression* e)
{
    // The AST doesn't maintain function invokation info,
    // so I am using the bitwise or as a workaround
    if (e->GetOperator() == TokenType::BitwiseOr)
    {
        InvokeFunction(e);
        return;
    }

    m_RegisterStack.push(GetRegisterForExpression(e->GetLeft().get()));
    e->GetLeft()->Accept(*this);

    m_RegisterStack.push(GetRegisterForExpression(e->GetRight().get()));
    e->GetRight()->Accept(*this);

    const auto second_reg = m_RegisterStack.top();
    m_RegisterStack.pop();
    const auto first_reg = m_RegisterStack.top();
    m_RegisterStack.pop();

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
        NormalizeBoolean();
    }
    else if (OperationIsLogical(op))
    {
        // p(logical op) xmm0, xmm1
        PushBytes(0x66, 0x0F, LogicalOperationToByte(op), 0xC1);
        NormalizeBoolean();
    }
    else
    {
        switch (op)
        {
            case TokenType::Equal:
            {
                // movsd QWORD PTR [rbp-0x11223344], xmm1
                PushBytes(0xF2, 0x0F, 0x11, 0x8D);
                Push4Bytes(GetDisplacement(first_reg));

                // movsd  xmm0, xmm1
                PushBytes(0xF2, 0x0F, 0x10, 0xC1);
            } break;
        }
    }

    // movq    QWORD PTR [rbp+0x0], xmm0
    PushBytes(0x66, 0x0F, 0xD6, 0x85);
    Push4Bytes(GetDisplacement(m_RegisterStack.top()));
}

void X64Generator::Visit(UnaryExpression* e)
{
    const auto reg = GetRegisterForExpression(e->GetExpr().get());
    m_RegisterStack.push(reg);
    e->GetExpr()->Accept(*this);
    m_RegisterStack.pop();

    switch (e->GetOperator())
    {
        // @TODO: don't emit jump if already at the end
        case TokenType::Return:
        {
            // movsd xmm0, QWORD PTR [bsp-0x00]
            PushBytes(0xF2, 0x0F, 0x10, 0x85);
            Push4Bytes(GetDisplacement(reg));

            // jmp to end of function
            m_ReturnFixupOffsets.push(m_Code.size());
            PushBytes(0xE9);
            Push4Bytes(0x00);
        } break;

        case TokenType::Minus:
        {
            // movsd  xmm0, QWORD PTR[rbp - 0x8]
            PushBytes(0xF2, 0x0F, 0x10, 0x85);
            Push4Bytes(GetDisplacement(reg));

            const int mask = GetNewRegister();
            MovRegNumberRaw(mask, SIGN_BIT_MASK_64);

            // movsd  xmm1, QWORD PTR [rbp-0x10]
            PushBytes(0xF2, 0x0F, 0x10, 0x8D);
            Push4Bytes(GetDisplacement(mask));

            // pxor xmm0, xmm1
            PushBytes(0x66, 0x0F, 0xEF, 0xC1);

            // movq    QWORD PTR [rbp+0x0], xmm0
            PushBytes(0x66, 0x0F, 0xD6, 0x85);
            Push4Bytes(GetDisplacement(m_RegisterStack.top()));
        } break;
    }
}

void X64Generator::Visit(VariableDefinitionExpression* e)
{
    PushIdentifier(e->GetName());
    
    m_RegisterStack.push(GetNewRegister());
    e->GetValue()->Accept(*this);

    const auto variable_reg = GetIdentifierRegister(e->GetName());
    const auto value_reg = m_RegisterStack.top();
    MovRegs(variable_reg, value_reg);
    m_RegisterStack.pop();
}

void X64Generator::Visit(BlockStatement* e)
{
    for (const auto& expr : e->GetValues())
    {
        expr->Accept(*this);
    }
}

void X64Generator::Visit(IfStatement* e)
{
    m_RegisterStack.push(GetNewRegister());
    e->GetCondition()->Accept(*this);

    JumpIfConditionIsFalse();
    {
        e->GetIfStatement()->Accept(*this);
        if (e->GetElseStatement())
        {
            BeginUnconditionalJumpForward();
        }
    }
    PatchConditionalJumpOffsets();

    if (e->GetElseStatement())
    {
        e->GetElseStatement()->Accept(*this);
        EndUnconditionalJumpForward();
    }
}

void X64Generator::Visit(WhileStatement* e)
{
    OpenBreakScope();
    OpenContinueScope();

    uintptr_t before_loop_offset = m_Code.size();

    BeginUnconditionalJumpBackward();
    m_RegisterStack.push(GetNewRegister());
    e->GetCondition()->Accept(*this);
    JumpIfConditionIsFalse();
    {
        e->GetBody()->Accept(*this);
        EndUnconditionalJumpBackward();
    }
    PatchConditionalJumpOffsets();

    CloseContinueScope(before_loop_offset);
    CloseBreakScope(m_Code.size());
}

void X64Generator::Visit(ForStatement* e)
{
    OpenBreakScope();
    OpenContinueScope();

    m_RegisterStack.push(GetNewRegister());
    e->GetInitialization()->Accept(*this);
    BeginUnconditionalJumpBackward();
    e->GetCondition()->Accept(*this);
    JumpIfConditionIsFalse();

    e->GetBody()->Accept(*this);
    uintptr_t iteration_offset = m_Code.size();
    e->GetIteration()->Accept(*this);
    EndUnconditionalJumpBackward();

    PatchConditionalJumpOffsets();

    CloseContinueScope(iteration_offset);
    CloseBreakScope(m_Code.size());
}

void X64Generator::Visit(Break* e)
{
    (void)e;
    m_BreakScopes.top().push_back(m_Code.size());

    // jump
    PushBytes(0xE9);
    Push4Bytes(0x00);
}

void X64Generator::Visit(Continue* e)
{
    (void)e;
    m_ContinueScopes.top().push_back(m_Code.size());

    // jump
    PushBytes(0xE9);
    Push4Bytes(0x00);
}

void X64Generator::Visit(ListExpression* e)
{
    for (const auto& expr : e->GetValues())
    {
        expr->Accept(*this);
    }
}

void X64Generator::Visit(FunctionDeclaration* e)
{
    // push    rbp
    // mov     rbp, rsp
    PushBytes(0x55, 0x48, 0x89, 0xE5);

    // sub rsp
    PushBytes(0x48, 0x81, 0xEC);
    Push4Bytes(0x00);

    uintptr_t rsp_sub_offset = m_Code.size() - 4;

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
    while (!m_ReturnFixupOffsets.empty())
    {
        const uintptr_t offset = m_ReturnFixupOffsets.top();
        Replace32BitsAtOffset(offset + 1, CalculateRelative32BitOffset(offset + 5, m_Code.size()));
        m_ReturnFixupOffsets.pop();
    }

    bool should_pad = m_NextRegister % 2 == 1; // to preserve 16-bit alignment
    Replace32BitsAtOffset(rsp_sub_offset, m_NextRegister * 8 + should_pad * 8);

    // add     rsp, 0x11223344
    PushBytes(0x48, 0x81, 0xC4);
    Push4Bytes(m_NextRegister * 8);

    // mov rsp, rbp
    PushBytes(0x48, 0x89, 0xEC);

    // pop  rbp
    // ret
    PushBytes(0x5D, 0xC3);
}

void X64Generator::Visit(TopStatements* e)
{
    for (const auto& statement : e->GetValues())
    {
        m_RegisterStack.push(GetNewRegister());
        statement->Accept(*this);
        m_RegisterStack.pop();
    }
}
