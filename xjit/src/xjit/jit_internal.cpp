#include "jit.h"
#include "jit_helpers.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

const Byte* X64Generator::GetExecutableMemory() const
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    const auto page_size = system_info.dwPageSize;

    Byte* buffer = (Byte*)VirtualAlloc(NULL, page_size, MEM_COMMIT, PAGE_READWRITE);
    if (buffer)
    {
        memcpy(buffer, executable_memory.data(), executable_memory.size());
        DWORD old_protection;
        VirtualProtect(buffer, page_size, PAGE_EXECUTE_READ, &old_protection);
    }

    return buffer;
}

void X64Generator::SetIdentifierRegister(const IPLString& name, int reg)
{
    identifier_to_register[name] = reg;
}

void X64Generator::PushIdentifier(const IPLString& name)
{
    if (identifier_to_register.find(name) == identifier_to_register.end())
    {
        identifier_to_register[name] = GetNewRegister();
    }
}


int X64Generator::GetIdentifierRegister(const IPLString& name)
{
    assert(identifier_to_register.find(name) != identifier_to_register.end() && "Identifier not pushed");
    return identifier_to_register.at(name);
}

void X64Generator::PushQWORD(uint64_t qword)
{
    for (int i = 0; i < sizeof(qword); ++i)
    {
        executable_memory.push_back((qword >> (i * 8)) & 0xFF);
    }
}

void X64Generator::PushDWORD(uint32_t dword)
{
    for (int i = 0; i < sizeof(dword); ++i)
    {
        executable_memory.push_back((dword >> (i * 8)) & 0xFF);
    }
}

void X64Generator::MovRegs(int to, int from)
{
    // movsd  xmm0,QWORD PTR [rbp-0x11223344]
    PushBytes(0xF2, 0x0F, 0x10, 0x85);
    PushDWORD(GetDisplacement(from));

    // movq   QWORD PTR [rbp-0x18],xmm0
    PushBytes(0x66, 0x0F, 0xD6, 0x85);
    PushDWORD(GetDisplacement(to));
}

void X64Generator::MovRegAddress(int reg, const void* address)
{
    // movabs rax, [absolute_address]
    PushBytes(0x48, 0x48, 0xA1);
    PushQWORD((uint64_t)address);

    // movq [rbp - 8], rax
    PushBytes(0x48, 0x89, 0x85);
    PushDWORD(GetDisplacement(reg));
}

void X64Generator::MovRegNumber(int reg, double number)
{
    const auto pair = literals.insert(number);
    const auto address = &*pair.first;
    MovRegAddress(reg, address);
}

void X64Generator::ReplaceDWORDAtOffset(uintptr_t offset, uint32_t dword)
{
    for (size_t i = 0; i < sizeof(dword); ++i)
    {
        executable_memory[offset + i] = ((dword >> (i * 8)) & 0xFF);
    }
}
