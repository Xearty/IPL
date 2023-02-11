#include "jit.h"
#include "jit_runtime.h"
#include "jit_helpers.h"

void X64Generator::ResetState()
{
    m_NextRegister = 1;
    IPLVector<Byte>().swap(m_Code);
    IPLStack<uintptr_t>().swap(m_UnconditionalJumpFixupOffsets);
    IPLStack<uintptr_t>().swap(m_ConditionalJumpFixupOffsets);
    IPLStack<uintptr_t>().swap(m_ReturnFixupOffsets);
    IPLStack<int>().swap(m_RegisterStack);
    IPLUnorderedMap<IPLString, int>().swap(m_IdentifierToRegisterMapping);
    IPLStack<IPLVector<uintptr_t>>().swap(m_BreakScopes);
    IPLStack<IPLVector<uintptr_t>>().swap(m_ContinueScopes);
}

void X64Generator::SetIdentifierRegister(const IPLString& name, int reg)
{
    m_IdentifierToRegisterMapping[name] = reg;
}

void X64Generator::PushIdentifier(const IPLString& name)
{
    assert(m_IdentifierToRegisterMapping.find(name) == m_IdentifierToRegisterMapping.end());
    if (m_IdentifierToRegisterMapping.find(name) == m_IdentifierToRegisterMapping.end())
    {
        m_IdentifierToRegisterMapping[name] = GetNewRegister();
    }
}

int X64Generator::GetIdentifierRegister(const IPLString& name)
{
    assert(m_IdentifierToRegisterMapping.find(name) != m_IdentifierToRegisterMapping.end() && "Identifier not pushed");
    return m_IdentifierToRegisterMapping.at(name);
}

void X64Generator::Push8Bytes(uint64_t qword)
{
    for (int i = 0; i < sizeof(qword); ++i)
    {
        m_Code.push_back((qword >> (i * 8)) & 0xFF);
    }
}

void X64Generator::Push4Bytes(uint32_t dword)
{
    for (int i = 0; i < sizeof(dword); ++i)
    {
        m_Code.push_back((dword >> (i * 8)) & 0xFF);
    }
}

void X64Generator::MovRegs(int to, int from)
{
    // movsd  xmm0,QWORD PTR [rbp-0x11223344]
    PushBytes(0xF2, 0x0F, 0x10, 0x85);
    Push4Bytes(GetDisplacement(from));

    // movq   QWORD PTR [rbp-0x18],xmm0
    PushBytes(0x66, 0x0F, 0xD6, 0x85);
    Push4Bytes(GetDisplacement(to));
}

void X64Generator::MovRegAddress(int reg, const void* address)
{
    // movabs rax, [absolute_address]
    PushBytes(0x48, 0xA1);
    Push8Bytes((uint64_t)address);

    // movq [rbp - 8], rax
    PushBytes(0x48, 0x89, 0x85);
    Push4Bytes(GetDisplacement(reg));
}

void X64Generator::MovRegNumber(int reg, double number)
{
    const auto pair = m_NumberLiteralSet.insert(number);
    const auto address = &*pair.first;
    MovRegAddress(reg, address);
}

void X64Generator::MovRegNumberRaw(int reg, uint64_t number)
{
    MovRegNumber(reg, *reinterpret_cast<double*>(&number));
}

void X64Generator::Replace32BitsAtOffset(uintptr_t offset, uint32_t dword)
{
    for (size_t i = 0; i < sizeof(dword); ++i)
    {
        m_Code[offset + i] = ((dword >> (i * 8)) & 0xFF);
    }
}

void X64Generator::JumpIfConditionIsFalse()
{
    assert(!m_RegisterStack.empty());

    // pxor xmm0, xmm0
    PushBytes(0x66, 0x0F, 0xEF, 0xC0);

    // ucomisd xmm0, QWORD PTR [rbp-0x11223344]
    PushBytes(0x66, 0x0F, 0x2E, 0x85);
    Push4Bytes(GetDisplacement(m_RegisterStack.top()));

    // check je and jp (equality and parity)
    //jp
    m_ConditionalJumpFixupOffsets.push(m_Code.size());
    PushBytes(0x0F, 0x8A);
    Push4Bytes(0x00);

    // ucomisd xmm0, QWORD PTR [rbp-0x11223344]
    PushBytes(0x66, 0x0F, 0x2E, 0x85);
    Push4Bytes(GetDisplacement(m_RegisterStack.top()));

    // je
    m_ConditionalJumpFixupOffsets.push(m_Code.size());
    PushBytes(0x0F, 0x84);
    Push4Bytes(0x00);

    m_RegisterStack.pop();
}

void X64Generator::PatchConditionalJumpOffsets()
{
    assert(m_ConditionalJumpFixupOffsets.size() >= 2);

    uintptr_t je_offset = m_ConditionalJumpFixupOffsets.top();
    PatchConditionalJump(je_offset, m_Code.size());
    m_ConditionalJumpFixupOffsets.pop();

    uintptr_t jp_offset = m_ConditionalJumpFixupOffsets.top();
    PatchConditionalJump(jp_offset, m_Code.size());
    m_ConditionalJumpFixupOffsets.pop();
}

void X64Generator::BeginUnconditionalJumpForward()
{
    m_UnconditionalJumpFixupOffsets.push(m_Code.size());
    PushBytes(0xE9);
    Push4Bytes(0x00);
}

void X64Generator::EndUnconditionalJumpForward()
{
    uintptr_t offset = m_UnconditionalJumpFixupOffsets.top();
    PatchUnconditionalJump(offset, m_Code.size());
    m_UnconditionalJumpFixupOffsets.pop();
}

void X64Generator::BeginUnconditionalJumpBackward()
{
    m_UnconditionalJumpFixupOffsets.push(m_Code.size());
}

void X64Generator::EndUnconditionalJumpBackward()
{
    UnconditionalJumpAtOffset(m_UnconditionalJumpFixupOffsets.top());
    m_UnconditionalJumpFixupOffsets.pop();
}

void X64Generator::UnconditionalJumpAtOffset(uintptr_t offset)
{
    PushBytes(0xE9);
    Push4Bytes(CalculateRelative32BitOffset(m_Code.size() + 4, offset));
}

void X64Generator::PatchUnconditionalJump(uintptr_t instruction_offset, uintptr_t jump_to)
{
    Replace32BitsAtOffset(instruction_offset + 1, CalculateRelative32BitOffset(instruction_offset + 5, jump_to));
}

void X64Generator::PatchConditionalJump(uintptr_t instruction_offset, uintptr_t jump_to)
{
    Replace32BitsAtOffset(instruction_offset + 2, CalculateRelative32BitOffset(instruction_offset + 6, jump_to));
}

void X64Generator::OpenContinueScope()
{
    m_ContinueScopes.emplace();;
}

void X64Generator::CloseContinueScope(uintptr_t to_jump_offset)
{
    assert(!m_ContinueScopes.empty());
    const auto& scope = m_ContinueScopes.top();

    for (const uintptr_t& jmp_offset : scope)
    {
        PatchUnconditionalJump(jmp_offset, to_jump_offset);
    }
    m_ContinueScopes.pop();
}

void X64Generator::OpenBreakScope()
{
    m_BreakScopes.emplace();
}

void X64Generator::CloseBreakScope(uintptr_t to_jump_offset)
{
    assert(!m_BreakScopes.empty());
    const auto& scope = m_BreakScopes.top();

    for (const uintptr_t& jmp_offset : scope)
    {
        PatchUnconditionalJump(jmp_offset, to_jump_offset);
    }
    m_BreakScopes.pop();
}

void X64Generator::NormalizeBoolean()
{
    const int one = GetNewRegister();
    MovRegNumberRaw(one, DOUBLE_ONE);

    // movsd  xmm1, QWORD PTR [rbp-0x10]
    PushBytes(0xF2, 0x0F, 0x10, 0x8D);
    Push4Bytes(GetDisplacement(one));

    // pand  xmm0, xmm1
    PushBytes(0x66, 0x0F, 0xDB, 0xC1);
}

int X64Generator::GetRegisterForExpression(Expression* e)
{
    // @TODO: Think of something more clever
    if (const auto identifier = dynamic_cast<IdentifierExpression*>(e))
    {
        assert(m_IdentifierToRegisterMapping.find(identifier->GetName()) != m_IdentifierToRegisterMapping.end());
        return GetIdentifierRegister(identifier->GetName());
    }

    if (const auto call = dynamic_cast<Call*>(e))
    {
        return GetRegisterForExpression(call->GetObjectOrCall().get());
    }

    return GetNewRegister();
}
