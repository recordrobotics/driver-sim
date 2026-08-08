#include "logger.h"

#include <SDL3/SDL.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>

#include <cstddef>

namespace blackboard::logger
{
    void init()
    {
        static constexpr size_t file_size{5ull * 1024ull * 1024ull};
        static constexpr size_t rotating_files{5u};
        static const std::filesystem::path filename{
            std::filesystem::path{SDL_GetPrefPath(nullptr, "DriverSim")} / "logs" /
            "driversim.log"};

        std::vector<spdlog::sink_ptr> sinks;
        std::vector<spdlog::spdlog_ex> exceptions;

        auto console_sink_trace = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        console_sink_trace->set_pattern("[%^%l%$] %v");
        sinks.push_back(console_sink_trace);

        try
        {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                filename.string(), file_size, rotating_files, true);

            sinks.push_back(file_sink);
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            exceptions.push_back(ex);
        }

        logger = std::make_shared<spdlog::logger>("driversim_log", sinks.begin(), sinks.end());
        logger::logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
        using namespace std::chrono_literals;
        spdlog::flush_every(1s);

        for (const auto &exc : exceptions)
        {
            logger->error("Failed to create log file sink: {}", exc.what());
        }
    }

    void shutdown() { spdlog::shutdown(); }

} // namespace blackboard::logger
