#pragma once
#include <windows.h>
#include <iphlpapi.h>
#include <vector>
#include <algorithm>

// A successful snapshot owns both its rows and their actual allocation capacity.
// Never reuse the API's returned required size with an older allocation.
class CIfTableSnapshot
{
public:
    using Query = DWORD (WINAPI*)(PMIB_IFTABLE, PULONG, BOOL);
    bool Refresh(Query query = ::GetIfTable)
    {
        std::vector<BYTE> next;
        ULONG capacity = static_cast<ULONG>((std::max)(m_data.size(), sizeof(MIB_IFTABLE)));
        m_error = ERROR_INSUFFICIENT_BUFFER;
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            if (capacity > 64 * 1024 * 1024)
                break;
            next.resize(capacity);
            ULONG size = capacity;
            m_error = query(reinterpret_cast<MIB_IFTABLE*>(next.data()), &size, FALSE);
            if (m_error == ERROR_INSUFFICIENT_BUFFER)
            {
                capacity = (std::max)(size, capacity * 2);
                continue;
            }
            if (m_error == ERROR_NO_DATA)
            {
                next.clear();
                m_error = NO_ERROR;
            }
            else if (m_error == NO_ERROR)
            {
                const auto count = reinterpret_cast<MIB_IFTABLE*>(next.data())->dwNumEntries;
                if (size > capacity || size < FIELD_OFFSET(MIB_IFTABLE, table) ||
                    count > (size - FIELD_OFFSET(MIB_IFTABLE, table)) / sizeof(MIB_IFROW))
                    m_error = ERROR_INVALID_DATA;
            }
            if (m_error == NO_ERROR)
            {
                auto table = next.empty() ? &m_empty : reinterpret_cast<MIB_IFTABLE*>(next.data());
                m_changed = !SameTopology(Get(), table);
                m_data.swap(next);
                return true;
            }
            break;
        }
        // A failed query must not expose partial, uninitialized or stale rows.
        m_changed = !m_data.empty();
        m_data.clear();
        return false;
    }
    MIB_IFTABLE* Get() { return m_data.empty() ? &m_empty : reinterpret_cast<MIB_IFTABLE*>(m_data.data()); }
    bool Changed() const { return m_changed; }
    DWORD Error() const { return m_error; }
private:
    static bool SameTopology(const MIB_IFTABLE* a, const MIB_IFTABLE* b)
    {
        if (a->dwNumEntries != b->dwNumEntries) return false;
        for (DWORD i = 0; i < a->dwNumEntries; ++i)
            if (a->table[i].dwIndex != b->table[i].dwIndex ||
                a->table[i].dwOperStatus != b->table[i].dwOperStatus ||
                a->table[i].dwDescrLen != b->table[i].dwDescrLen ||
                memcmp(a->table[i].bDescr, b->table[i].bDescr,
                    (std::min)(a->table[i].dwDescrLen, DWORD(MAXLEN_IFDESCR))) != 0)
                return false;
        return true;
    }
    std::vector<BYTE> m_data;
    MIB_IFTABLE m_empty{};
    bool m_changed{};
    DWORD m_error{};
};
