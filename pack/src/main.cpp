#include "main.hpp"
#include "pack.hpp"
#include "util.hpp"

#include <CLI/CLI.hpp>
#include <lzokay.hpp>
#include <mio/mmap.hpp>
#include <spdlog/async.h>
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <system_error>
#include <thread>

namespace {

// ---------------------------------------------------------
// Command: extract
// ---------------------------------------------------------

namespace commands::extract {

struct Args
{
    std::vector<std::filesystem::path> paths;
    std::filesystem::path dest = ".";
    std::string filter;
    size_t num_threads = std::thread::hardware_concurrency() + 1;
};

// NOLINTNEXTLINE(*-function-cognitive-complexity)
void exec(const Args& args)
{
    std::optional<std::regex> filter;
    if (!args.filter.empty()) {
        filter = std::regex(args.filter);
    }

    for (const auto& path : args.paths) {
        const auto pack = Pack::open(path);

        std::vector<std::thread> threads;
        threads.reserve(args.num_threads);
        std::mutex console_mutex;

        for (size_t i_thread = 0; i_thread < args.num_threads; ++i_thread) {
            threads.emplace_back([&, offset = i_thread] {
                const auto num_files_total = pack.files().size();
                for (size_t i_file = offset; i_file < num_files_total; i_file += args.num_threads) {
                    const auto& file = pack.files().at(i_file);
                    if (filter.has_value() && !std::regex_match(file.path().string(), filter.value())) {
                        continue;
                    }

                    const auto output_path = (args.dest / file.path()).lexically_normal();

                    {
                        auto lock = std::lock_guard{console_mutex};
                        std::cout << std::format("{} => {}\n", pack.name(), output_path.string());
                    }

                    {
                        auto current_dir = output_path.parent_path();
                        if (!std::filesystem::exists(current_dir)) {
                            std::error_code error;
                            std::filesystem::create_directories(current_dir, error);
                            if (error) {
                                throw Error("Failed to create directory: {}: {}", current_dir.string(), error.message());
                            }
                        }
                    }

                    // NOLINTNEXTLINE(*-signed-bitwise)
                    std::ofstream output{output_path, std::ios::out | std::ios::trunc | std::ios::binary};
                    if (!output) {
                        throw Error("Failed to open {}", file.path().string());
                    }

                    size_t output_size = 0;
                    for (const auto& chunk : file.chunks()) {
                        static constexpr size_t LZO_BUFFER_SIZE = 65536;
                        const bool decompress =
                            (chunk.size() < LZO_BUFFER_SIZE) && (output_size + chunk.size() < file.size());

                        if (decompress) [[likely]] {
                            // NOLINTNEXTLINE(*-member-init)
                            std::array<uint8_t, LZO_BUFFER_SIZE> lzo_buffer;
                            size_t decompressed_size = 0;
                            // NOLINTNEXTLINE(*-reinterpret-cast)
                            if (lzokay::decompress(reinterpret_cast<const uint8_t*>(chunk.begin()),
                                                   chunk.size(),
                                                   lzo_buffer.data(),
                                                   LZO_BUFFER_SIZE,
                                                   decompressed_size) != lzokay::EResult::Success) {
                                throw Error("{}: LZO decompression failed", file.path().string());
                            }

                            // NOLINTNEXTLINE(*-reinterpret-cast)
                            output.write(reinterpret_cast<const char*>(lzo_buffer.data()),
                                         static_cast<std::streamsize>(decompressed_size));
                            output_size += decompressed_size;
                        } else {
                            // NOLINTNEXTLINE(*-reinterpret-cast)
                            output.write(reinterpret_cast<const char*>(chunk.begin()), chunk.size());
                            output_size += chunk.size();
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
// Command: list
// ---------------------------------------------------------

namespace commands::list {

struct Args
{
    std::vector<std::filesystem::path> paths;
    std::string filter;
};

void exec(const Args& args)
{
    std::optional<std::regex> filter;
    if (!args.filter.empty()) {
        filter = std::regex(args.filter);
    }

    for (const auto& path : args.paths) {
        const auto pack = Pack::open(path);
        for (const auto& file : pack.files()) {
            if (!filter.has_value() || std::regex_match(file.path().string(), filter.value())) {
                std::cout << std::format("{}: {}\n", pack.name(), file.path().string());
            }
        }
    }
}

}

}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

auto main(int argc, const char* argv[]) -> int
try {
    auto stderr_logger = spdlog::stderr_color_mt("pack");
    spdlog::set_default_logger(stderr_logger);
    spdlog::set_pattern("[%^%l%$] %v");
    spdlog::set_level(spdlog::level::warn);

    constexpr auto VERSION = build::version();

    CLI::App cli;
    cli.description("M2TW pack manipulation tool");
    cli.name("pack");
    cli.set_version_flag("-v, --version",
                         std::format("{}.{}.{}", VERSION.major, VERSION.minor, VERSION.patch),
                         "Print version information and exit");
    cli.failure_message(CLI::FailureMessage::simple);
    cli.require_subcommand(1);
    cli.positionals_at_end();
    cli.validate_positionals();

    commands::extract::Args extract_args;
    auto* extract_cmd = cli.add_subcommand("extract", "Extract files from pack");
    extract_cmd->add_option("--dest", extract_args.dest, "Output directory")->check(CLI::ExistingDirectory);
    extract_cmd->add_option("--filter", extract_args.filter, "Regex for files to extract");
    extract_cmd->add_option("--parallel", extract_args.num_threads, "Number of threads");
    extract_cmd->add_option("PACK", extract_args.paths, "Pack to extract")->check(CLI::ExistingFile);

    commands::list::Args list_args;
    auto* list_cmd = cli.add_subcommand("list", "List files in pack");
    list_cmd->add_option("--filter", list_args.filter, "Regex for files to list");
    list_cmd->add_option("PACK", list_args.paths, "Pack to list")->check(CLI::ExistingFile);

    try {
        cli.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return cli.exit(e);
    }

    for (auto* command : cli.get_subcommands()) {
        if (command == extract_cmd) {
            commands::extract::exec(extract_args);
        } else if (command == list_cmd) {
            commands::list::exec(list_args);
        }
    }

    return EXIT_SUCCESS;
} catch (const Error& e) {
    spdlog::error("{}", e.what());
    return EXIT_FAILURE;
} catch (const std::exception& e) {
    spdlog::critical("An unexpected exception occurred: {}", e.what());
    return EXIT_FAILURE;
}
