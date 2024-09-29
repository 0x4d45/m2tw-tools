#pragma once

#include <mio/mmap.hpp>

#include <cassert>
#include <expected>
#include <filesystem>

class Chunk
{
public:
    explicit Chunk(const void* data, uint32_t size);
    auto begin() const -> const void*;
    auto end() const -> const void*;
    auto size() const -> uint32_t;

    static constexpr uint32_t MAX_SIZE = 65536;

private:
    const uint8_t* m_data;
    uint32_t m_size;
};

class File
{
public:
    explicit File(std::filesystem::path path, uint32_t size, std::vector<Chunk> chunks);
    auto path() const -> const std::filesystem::path&;
    auto size() const -> uint32_t;
    auto chunks() const -> const std::vector<Chunk>&;

private:
    std::filesystem::path m_path;
    uint32_t m_size;
    std::vector<Chunk> m_chunks;
};

class Pack
{
public:
    static auto open(std::filesystem::path path) -> Pack;
    auto path() const -> const std::filesystem::path&;
    auto name() const -> std::string;
    auto files() const -> const std::vector<File>&;

private:
    explicit Pack(std::filesystem::path path, std::unique_ptr<mio::mmap_source> mmap, std::vector<File> files);

    std::filesystem::path m_path;
    std::unique_ptr<mio::mmap_source> m_mmap;
    std::vector<File> m_files;
};
