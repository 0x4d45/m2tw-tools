#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

class Error : public std::runtime_error
{
public:
    template<typename... T>
    explicit Error(std::format_string<T...> fmt, T&&... args)
        : std::runtime_error(std::format(fmt, std::forward<T>(args)...))
    {
    }
};

class BinaryReader
{
public:
    explicit BinaryReader(const void* data, size_t size);
    auto pos() const -> size_t;
    void seek(size_t offset);
    void skip(size_t amount);
    void align(size_t boundary);
    auto get_u8() -> uint8_t;
    auto get_u32() -> uint32_t;
    auto get_c_str() -> std::string;

private:
    const uint8_t* m_begin;
    const uint8_t* m_end;
    const uint8_t* m_head;
};
