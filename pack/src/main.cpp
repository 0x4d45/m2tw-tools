#include <argparse/argparse.hpp>
#include <glob/glob.hpp>
#include <lzokay.hpp>
#include <mio/mio.hpp>

#include <array>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

// ---------------------------------------------------------

namespace {

namespace console {

constexpr std::string_view ANSI_RESET = "\033[0m";
constexpr std::string_view ANSI_RED = "\033[0;31m";
constexpr std::string_view ANSI_GREEN = "\033[0;32m";
constexpr std::string_view ANSI_YELLOW = "\033[0;33m";
constexpr std::string_view ANSI_BLUE = "\033[0;34m";
constexpr std::string_view ANSI_BG_RED = "\033[0;41m";

template<typename... T>
void debug(std::format_string<T...> fmt, T&&... args)
{
    auto msg = std::format(fmt, std::forward<T>(args)...);
    std::cerr << std::format("[{}DEBUG{}] {}\n", ANSI_BLUE, ANSI_RESET, msg);
}

template<typename... T>
void info(std::format_string<T...> fmt, T&&... args)
{
    auto msg = std::format(fmt, std::forward<T>(args)...);
    std::cerr << std::format("[{}INFO{}] {}\n", ANSI_GREEN, ANSI_RESET, msg);
}

template<typename... T>
void warn(std::format_string<T...> fmt, T&&... args)
{
    auto msg = std::format(fmt, std::forward<T>(args)...);
    std::cerr << std::format("[{}WARN{}] {}\n", ANSI_YELLOW, ANSI_RESET, msg);
}

template<typename... T>
void error(std::format_string<T...> fmt, T&&... args)
{
    auto msg = std::format(fmt, std::forward<T>(args)...);
    std::cerr << std::format("[{}ERROR{}] {}\n", ANSI_RED, ANSI_RESET, msg);
}

template<typename... T>
void fatal(std::format_string<T...> fmt, T&&... args)
{
    auto msg = std::format(fmt, std::forward<T>(args)...);
    std::cerr << std::format("[{}FATAL{}] {}\n", ANSI_BG_RED, ANSI_RESET, msg);
}

}

// ---------------------------------------------------------

class Error : public std::runtime_error
{
public:
    template<typename... T>
    explicit Error(std::format_string<T...> fmt, T&&... args)
        : std::runtime_error(std::format(fmt, std::forward<T>(args)...))
    {
    }
};

// ---------------------------------------------------------

struct Chunk
{
    uint32_t offset;
    uint32_t size;
};

struct File
{
    std::string path;
    uint32_t size_on_disk;
    uint32_t size_in_pack;
    std::vector<Chunk> chunks;
};

struct Pack
{
    std::unique_ptr<mio::mmap_source> mmap;
    std::string path;
    std::string name;
    std::vector<File> files;
};

auto load_pack(const std::string& path) -> Pack
{
    if (!std::filesystem::exists(path)) {
        throw Error("No such file: {}", path);
    }

    if (!std::filesystem::is_regular_file(path)) {
        throw Error("Not a file: {}", path);
    }

    console::debug("Loading {}", path);
    Pack pack;
    pack.path = path;
    pack.name = std::filesystem::path(path).filename().string();

    pack.mmap = std::make_unique<mio::mmap_source>();
    std::error_code err;
    pack.mmap->map(path, err);
    if (err) {
        throw Error("Failed to load {}: {}", path, err.message());
    }

    const auto* src = reinterpret_cast<const uint8_t*>(pack.mmap->begin());

    auto magic = *reinterpret_cast<const uint32_t*>(src);
    constexpr uint32_t PACK_MAGIC = 0x4b434150;
    if (magic != PACK_MAGIC) {
        throw Error("{}: Not a PACK file", path);
    }
    src += 4;

    auto version = *reinterpret_cast<const uint32_t*>(src);
    constexpr uint32_t EXPECTED_VERSION = 0x30000;
    if (version != EXPECTED_VERSION) {
        throw Error("{}: Unexpected PACK version: 0x{:x}", path, version);
    }
    src += 4;

    const auto num_files = *reinterpret_cast<const uint32_t*>(src);
    src += 4;

    const auto file_table_size = *reinterpret_cast<const uint32_t*>(src);
    src += 4;

    const auto num_chunks = *reinterpret_cast<const uint32_t*>(src);
    src += 4;

    std::vector<uint32_t> file_offsets;
    file_offsets.reserve(num_files);
    for (uint32_t i = 0; i < num_files; ++i) {
        file_offsets.push_back(*reinterpret_cast<const uint32_t*>(src));
        src += 4;
    }

    std::vector<uint32_t> chunk_sizes;
    chunk_sizes.reserve(num_chunks);
    for (uint32_t i = 0; i < num_chunks; ++i) {
        chunk_sizes.push_back(*reinterpret_cast<const uint32_t*>(src));
        src += 4;
    }

    for (uint32_t i = 0; i < num_files; ++i) {
        const auto data_offset = *reinterpret_cast<const uint32_t*>(src);
        src += 4;

        const auto first_chunk = *reinterpret_cast<const uint32_t*>(src);
        src += 4;

        const auto size_on_disk = *reinterpret_cast<const uint32_t*>(src);
        src += 4;

        const auto size_in_pack = *reinterpret_cast<const uint32_t*>(src);
        src += 4;

        const char* const path = reinterpret_cast<const char*>(src);
        src += strlen(path) + 1;

        File file;
        file.path = path;
        file.size_on_disk = size_on_disk;
        file.size_in_pack = size_in_pack;

        auto chunk_index = first_chunk;
        auto chunk_offset = data_offset;
        while (chunk_offset - data_offset < size_in_pack) {
            auto chunk_size = chunk_sizes[chunk_index];
            file.chunks.emplace_back(Chunk{
                .offset = chunk_offset,
                .size = chunk_size,
            });
            chunk_offset += chunk_size;
            chunk_index += 1;
        }

        pack.files.push_back(std::move(file));

        // Align to 4.
        while ((reinterpret_cast<const char*>(src) - pack.mmap->begin()) % 4 != 0) {
            ++src;
        }
    }

    return pack;
};

// ---------------------------------------------------------

void cmd_list(const std::vector<std::string>& paths)
{
    for (const auto& path : paths) {
        const auto pack = load_pack(path);
        for (const auto& file : pack.files) {
            std::cout << file.path << "\n";
        }
    }
}

void cmd_extract(const std::vector<std::string>& paths)
{
    const size_t num_threads = std::thread::hardware_concurrency();
    console::debug("Using {} threads", num_threads);

    for (const auto& path : paths) {
        const auto pack = load_pack(path);

        std::mutex console_mutex;
        std::mutex directory_mutex;
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([id = i, num_threads, &console_mutex, &directory_mutex, &pack]() {
                static constexpr size_t LZO_BUFFER_SIZE = 65536;
                std::array<uint8_t, LZO_BUFFER_SIZE> decompression_buffer{};

                const auto num_files_in_pack = pack.files.size();
                for (size_t j = id; j < num_files_in_pack; j += num_threads) {
                    const auto& file = pack.files.at(j);

                    {
                        auto lock = std::lock_guard{console_mutex};
                        console::info("{} => {}", pack.name, file.path);
                    }

                    {
                        auto current_dir = std::filesystem::path(file.path).parent_path();
                        auto lock = std::lock_guard{directory_mutex};
                        if (!std::filesystem::exists(current_dir)) {
                            std::error_code err;
                            std::filesystem::create_directories(current_dir, err);
                            if (err) {
                                throw Error("Failed to create directory: {}: {}", current_dir.string(), err.message());
                            }
                        }
                    }

                    std::ofstream file_out{file.path, std::ios::out | std::ios::trunc | std::ios::binary};
                    if (!file_out) {
                        throw Error("Failed to open {}", file.path);
                    }

                    size_t bytes_out = 0;
                    for (const auto& chunk : file.chunks) {
                        const uint8_t* chunk_data = reinterpret_cast<const uint8_t*>(pack.mmap->begin()) + chunk.offset;

                        const bool chunk_is_compressed =
                            (chunk.size < decompression_buffer.size()) && (bytes_out + chunk.size != file.size_on_disk);

                        [[likely]] if (chunk_is_compressed) {
                            size_t decompressed_size = 0;
                            if (lzokay::decompress(chunk_data,
                                                   chunk.size,
                                                   decompression_buffer.data(),
                                                   LZO_BUFFER_SIZE,
                                                   decompressed_size) != lzokay::EResult::Success) {
                                throw Error("{}: LZO decompression failed", file.path);
                            }

                            file_out.write(reinterpret_cast<const char*>(decompression_buffer.data()),
                                           decompressed_size);
                            bytes_out += decompressed_size;
                        } else {
                            file_out.write(reinterpret_cast<const char*>(chunk_data), chunk.size);
                            bytes_out += chunk.size;
                        }
                    }
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }
}

}

// ---------------------------------------------------------

auto main(int argc, char* argv[]) -> int
try {
    // Performance optimization. Note that this means we must not use the C stdio API.
    std::ios::sync_with_stdio(false);

    auto program = argparse::ArgumentParser("pack");

    auto list_cmd = argparse::ArgumentParser("list");
    list_cmd.add_argument("files").remaining();

    auto extract_cmd = argparse::ArgumentParser("extract");
    extract_cmd.add_argument("files").remaining();

    program.add_subparser(list_cmd);
    program.add_subparser(extract_cmd);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& e) {
        throw Error("{}", e.what());
    }

    if (program.is_subcommand_used(list_cmd)) {
        const auto files = list_cmd.get<std::vector<std::string>>("files");
        cmd_list(files);
    } else if (program.is_subcommand_used(extract_cmd)) {
        const auto files = extract_cmd.get<std::vector<std::string>>("files");
        cmd_extract(files);
    }

    return EXIT_SUCCESS;
} catch (const Error& e) {
    console::error("{}", e.what());
    return EXIT_FAILURE;
} catch (const std::exception& e) {
    console::fatal("An unexpected exception occurred: {}", e.what());
    return EXIT_FAILURE;
}
