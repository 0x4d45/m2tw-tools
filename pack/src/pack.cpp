#include "pack.hpp"
#include "util.hpp"

#include <mio/mmap.hpp>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// ---------------------------------------------------------

Chunk::Chunk(const void* data, uint32_t size)
    : m_data(std::bit_cast<uint8_t*>(data))
    , m_size(size)
{
}

auto Chunk::begin() const -> const void*
{
    return m_data;
}

auto Chunk::end() const -> const void*
{
    // NOLINTNEXTLINE(*-pointer-arithmetic)
    return m_data + m_size;
}

auto Chunk::size() const -> uint32_t
{
    return m_size;
}

// ---------------------------------------------------------

File::File(std::filesystem::path path, uint32_t size, std::vector<Chunk> chunks)
    : m_path(std::move(path))
    , m_size(size)
    , m_chunks(std::move(chunks))
{
}

auto File::path() const -> const std::filesystem::path&
{
    return m_path;
}

auto File::size() const -> uint32_t
{
    return m_size;
}

auto File::chunks() const -> const std::vector<Chunk>&
{
    return m_chunks;
}

// ---------------------------------------------------------

auto Pack::open(std::filesystem::path path) -> Pack
{
    auto mmap = std::make_unique<mio::mmap_source>();
    std::error_code error;
    mmap->map(path.string(), error);
    if (error) {
        throw Error("Failed to open file: {}: {}", path.string(), error.message());
    }

    auto reader = BinaryReader(mmap->begin(), mmap->size());

    constexpr uint32_t EXPECTED_MAGIC = 0x4b434150;
    const auto magic = reader.get_u32();
    if (magic != EXPECTED_MAGIC) {
        throw Error("Not a pack file: {}", path.string());
    }

    constexpr uint32_t EXPECTED_VERSION = 0x30000;
    const auto version = reader.get_u32();
    if (version != EXPECTED_VERSION) {
        throw Error("Unexpected pack version: {}: 0x{:x}", path.string(), version);
    }

    const auto num_files = reader.get_u32();
    [[maybe_unused]] const auto file_table_size = reader.get_u32();
    const auto num_chunks = reader.get_u32();

    std::vector<uint32_t> file_offsets;
    file_offsets.reserve(num_files);
    for (uint32_t i = 0; i < num_files; ++i) {
        file_offsets.push_back(reader.get_u32());
    }

    std::vector<uint32_t> chunk_sizes;
    chunk_sizes.reserve(num_chunks);
    for (uint32_t i = 0; i < num_chunks; ++i) {
        chunk_sizes.push_back(reader.get_u32());
    }

    std::vector<File> files;
    files.reserve(num_files);
    for (uint32_t i = 0; i < num_files; ++i) {
        const uint32_t first_chunk_offset = reader.get_u32();
        const uint32_t first_chunk_index = reader.get_u32();
        const uint32_t size_on_disk = reader.get_u32();
        const uint32_t size_in_pack = reader.get_u32();
        std::filesystem::path file_path = reader.get_c_str();
        reader.align(4);

        std::vector<Chunk> chunks;
        auto chunk_index = first_chunk_index;
        auto chunk_offset = first_chunk_offset;
        while (chunk_offset - first_chunk_offset < size_in_pack) {
            auto chunk_size = chunk_sizes[chunk_index];
            chunks.emplace_back(mmap->begin() + chunk_offset, chunk_size);
            chunk_index += 1;
            chunk_offset += chunk_size;
        }

        files.emplace_back(std::move(file_path), size_on_disk, std::move(chunks));
    }

    return Pack(std::move(path), std::move(mmap), std::move(files));
}

auto Pack::path() const -> const std::filesystem::path&
{
    return m_path;
}

auto Pack::name() const -> std::string
{
    return path().filename().string();
}

auto Pack::files() const -> const std::vector<File>&
{
    return m_files;
}

Pack::Pack(std::filesystem::path path, std::unique_ptr<mio::mmap_source> mmap, std::vector<File> files)
    : m_path(std::move(path))
    , m_mmap(std::move(mmap))
    , m_files(std::move(files))
{
}
