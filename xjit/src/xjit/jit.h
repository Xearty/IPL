#pragma once

#include <Expression.h>
#include <unordered_set>

using Byte = uint8_t;

class X64Generator : public ExpressionVisitor
{
public:
    X64Generator();

    const Byte* CompileFunction(Expression* e);
    const IPLVector<Byte>& GetMachineCode() const { return executable_memory; }

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
    // virtual void Visit(Break* e) override { (void)e; }
    // virtual void Visit(Continue* e) override { (void)e; }
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

    void JumpIfConditionIsFalse();
    void PatchConditionalJumpOffsets();

    void BeginUnconditionalJumpForwards();
    void EndUnconditionalJumpForwards();

    void BeginUnconditionalJumpBackwards();
    void EndUnconditionalJumpBackwards();

    template <typename... Rest>
    void PushBytes(Rest... rest)
    {
        (executable_memory.push_back(static_cast<Byte>(rest)), ...);
    }

    int GetNewRegister() { return next_register++; }
    int GetRegisterForExpression(Expression* e);

private:
    int next_register;
    IPLVector<Byte> executable_memory;
    IPLStack<uintptr_t> unconditional_jump_fixup_offsets;
    IPLStack<uintptr_t> jump_fixup_offsets;
    IPLStack<uintptr_t> return_fixup_offsets;
    IPLStack<int> registers;
    IPLUnorderedMap<IPLString, int> identifier_to_register;
    std::unordered_set<double> literals;
};
