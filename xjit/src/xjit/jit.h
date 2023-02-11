#pragma once

#include <Expression.h>
#include <unordered_set>

using Byte = uint8_t;

class X64Generator : public ExpressionVisitor
{
public:
    X64Generator();

    const Byte* CompileFunction(Expression* e);
    const IPLVector<Byte>& GetMachineCode() const { return m_Code; }

    virtual void Visit(LiteralNull* e) override;
    virtual void Visit(LiteralUndefined* e) override;
    // virtual void Visit(LiteralString* e) override { (void)e; }
    virtual void Visit(LiteralNumber* e) override;
    virtual void Visit(LiteralBoolean* e) override;
    // virtual void Visit(LiteralField* e) override { (void)e; }
    // virtual void Visit(LiteralObject* e) override { (void)e; }
    virtual void Visit(BinaryExpression* e) override;
    virtual void Visit(UnaryExpression* e) override;
    virtual void Visit(IdentifierExpression* e) override;
    virtual void Visit(ListExpression* e) override;
    virtual void Visit(VariableDefinitionExpression* e) override;
    virtual void Visit(BlockStatement* e) override;
    // virtual void Visit(LabeledStatement* e) override { (void)e; }
    virtual void Visit(IfStatement* e) override;
    // virtual void Visit(SwitchStatement* e) override { (void)e; }
    // virtual void Visit(CaseStatement* e) override { (void)e; }
    virtual void Visit(WhileStatement* e) override;
    virtual void Visit(ForStatement* e) override;
    virtual void Visit(Break* e) override;
    virtual void Visit(Continue* e) override;
    virtual void Visit(FunctionDeclaration* e) override;
    virtual void Visit(TopStatements* e) override;
    // virtual void Visit(EmptyExpression* e) override { (void)e; }
    virtual void Visit(Call* e) override;
    // virtual void Visit(MemberAccess* e) override { (void)e; }

private:
    // platform specific
    const Byte* GetExecutableMemory() const;

    void ResetState();

    void InitializeRuntime();
    // Using the bitwise or operator to invoke a funciton from the runtime
    void InvokeFunction(BinaryExpression* e);

    // value in xmm0 becomes double 1 if non-zero
    void NormalizeBoolean();

    void PushIdentifier(const IPLString& name);
    int GetIdentifierRegister(const IPLString& name);
    void SetIdentifierRegister(const IPLString& name, int reg);
    
    void Push8Bytes(uint64_t qword);
    void Push4Bytes(uint32_t dword);

    void MovRegs(int to, int from);
    void MovRegAddress(int reg, const void* address);
    void MovRegNumber(int reg, double number);
    void MovRegNumberRaw(int reg, uint64_t number);

    void Replace32BitsAtOffset(uintptr_t offset, uint32_t dword);
    void PatchUnconditionalJump(uintptr_t instruction_offset, uintptr_t jump_to);
    void PatchConditionalJump(uintptr_t instruction_offset, uintptr_t jump_to);

    void JumpIfConditionIsFalse();
    void PatchConditionalJumpOffsets();

    void BeginUnconditionalJumpForward();
    void EndUnconditionalJumpForward();

    void BeginUnconditionalJumpBackward();
    void EndUnconditionalJumpBackward();

    void UnconditionalJumpAtOffset(uintptr_t offset);

    void OpenContinueScope();
    void CloseContinueScope(uintptr_t to_jump_offset);

    void OpenBreakScope();
    void CloseBreakScope(uintptr_t to_jump_offset);

    template <typename... Rest>
    void PushBytes(Rest... rest)
    {
        (m_Code.push_back(static_cast<Byte>(rest)), ...);
    }

    int GetNewRegister() { return m_NextRegister++; }
    int GetRegisterForExpression(Expression* e);

private:
    IPLStack<IPLVector<uintptr_t>> m_ContinueScopes;
    IPLStack<IPLVector<uintptr_t>> m_BreakScopes;

    IPLStack<uintptr_t> m_UnconditionalJumpFixupOffsets;
    IPLStack<uintptr_t> m_ConditionalJumpFixupOffsets;
    IPLStack<uintptr_t> m_ReturnFixupOffsets;

    IPLUnorderedMap<IPLString, int> m_IdentifierToRegisterMapping;
    IPLUnorderedSet<double> m_NumberLiteralSet;

    int m_NextRegister;
    IPLStack<int> m_RegisterStack;

    IPLVector<Byte> m_Code;
};
