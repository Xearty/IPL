#pragma once

#include <Expression.h>
#include <spasm.hpp>
#include <memory>
#include <stdarg.h>

using Byte = uint8_t;

class X64Generator : public ExpressionVisitor
{
public:
    X64Generator();
    ~X64Generator() {}

    // virtual void Visit(LiteralNull* e) override { (void)e; }
    // virtual void Visit(LiteralUndefined* e) override { (void)e; }
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
    // virtual void Visit(WhileStatement* e) override { (void)e; }
    // virtual void Visit(ForStatement* e) override { (void)e; }
    // virtual void Visit(Break* e) override { (void)e; }
    // virtual void Visit(Continue* e) override { (void)e; }
    virtual void Visit(FunctionDeclaration* e) override;
    virtual void Visit(TopStatements* e) override;
    // virtual void Visit(EmptyExpression* e) override { (void)e; }
    virtual void Visit(Call* e) override;
    // virtual void Visit(MemberAccess* e) override { (void)e; }

    const IPLVector<Byte>& GetMachineCode() const { return executable_memory; }
    const Byte* CompileFunction(Expression* e);

private:
    const Byte* GetExecutableMemory() const;

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

    void JumpIfCondition();
    void PatchConditionalJump();

    template <typename... Rest>
    void PushBytes(Rest... rest)
    {
        (executable_memory.push_back(static_cast<Byte>(rest)), ...);
    }

    int GetNewRegister() { return next_register++; }

private:
    int next_register = 1;
    IPLVector<Byte> executable_memory;
    IPLStack<uintptr_t> jump_fixup_offsets; // contains jump offsets for if, switch and loops
    IPLStack<uintptr_t> return_fixup_offsets; // contains the jump offsets for return statements
    IPLStack<int> registers;
    IPLUnorderedMap<IPLString, int> identifier_to_register;
};
