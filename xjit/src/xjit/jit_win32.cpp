#include "jit.h"

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
        memcpy(buffer, m_Code.data(), m_Code.size());
        DWORD old_protection;
        VirtualProtect(buffer, page_size, PAGE_EXECUTE_READ, &old_protection);
    }

    return buffer;
}
