#include "util.hpp"

#include <cstdint>
#include <cstddef>
#include <string>

// NOLINTBEGIN(*-magic-numbers)
// NOLINTBEGIN(*-pointer-arithmetic)
// NOLINTBEGIN(*-reinterpret-cast)

BinaryReader::BinaryReader(const void* data, size_t size)
    : m_begin(reinterpret_cast<const uint8_t*>(data))
    , m_end(m_begin + size)
    , m_head(m_begin)
{
}

auto BinaryReader::pos() const -> size_t
{
    return m_head - m_begin;
}

void BinaryReader::seek(size_t offset)
{
    m_head = m_begin + offset;
}

void BinaryReader::skip(size_t amount)
{
    m_head += amount;
}

void BinaryReader::align(size_t boundary)
{
    while (pos() % boundary != 0) {
        skip(1);
    }
}

auto BinaryReader::get_u8() -> uint8_t
{
    const uint8_t result = *m_head;
    ++m_head;
    return result;
}

auto BinaryReader::get_u32() -> uint32_t
{
    auto result = static_cast<uint32_t>(get_u8());
    result |= static_cast<uint32_t>(get_u8()) << 8U;
    result |= static_cast<uint32_t>(get_u8()) << 16U;
    result |= static_cast<uint32_t>(get_u8()) << 24U;
    return result;
}

auto BinaryReader::get_c_str() -> std::string
{
    std::string result;
    while (*m_head != '\0') {
        result += static_cast<char>(*m_head);
        ++m_head;
    }
    ++m_head;
    return result;
}

// NOLINTEND(*-reinterpret-cast)
// NOLINTEND(*-pointer-arithmetic)
// NOLINTEND(*-magic-numbers)
