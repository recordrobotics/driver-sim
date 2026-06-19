#include "processrunner.h"

#include <SDL3/SDL.h>

bool ProcessRunner::start()
{
    process = reproc::process();

    options.working_directory = config.working_directory.c_str();
    options.redirect.out.type = reproc::redirect::pipe;
    options.redirect.err.type = reproc::redirect::pipe;
    options.env.behavior = reproc::env::extend;
    options.env.extra = config.environment;

    std::error_code ec = process.start(config.commandLine, options);
    if (ec)
    {
        logger->error("Failed to start process: {}, {}", ec.message(), config.commandLine[0]);
        return false;
    }

    logger->info("Process started successfully. PID: {}:{}, {}", process.pid().first, process.pid().second.message(), config.commandLine[0]);

    worker_thread = std::jthread([this](std::stop_token stop_token)
                                 {
        bool is_restart = false;

        while (!stop_token.stop_requested())
        {
            if (is_restart)
            {
                logger->info("Auto-restarting process: {}", config.commandLine[0]);
                process = reproc::process();
                std::error_code start_ec = process.start(config.commandLine, options);
                if (start_ec)
                {
                    logger->error("Failed to auto-restart process: {}, {}. Retrying in 2 seconds...", start_ec.message(), config.commandLine[0]);

                    auto fail_delay = std::chrono::seconds(2);
                    auto start_time = std::chrono::steady_clock::now();
                    while (!stop_token.stop_requested() && (std::chrono::steady_clock::now() - start_time) < fail_delay)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    continue;
                }
                logger->info("Process auto-restarted successfully. PID: {}:{}, {}", process.pid().first, process.pid().second.message(), config.commandLine[0]);
            }

            is_restart = true;

            std::string stdout_buffer;
            std::string stderr_buffer;

            // aggregate stream chunks into complete lines
            auto flush_lines = [](std::string &buffer, const uint8_t *bytes, size_t size,
                                  const std::function<void(const std::string &)> &log_func)
            {
                buffer.append(reinterpret_cast<const char *>(bytes), size);
                size_t pos;
                while ((pos = buffer.find('\n')) != std::string::npos)
                {
                    std::string line = buffer.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back(); // strip windows carriage return
                    }
                    log_func(line);
                    buffer.erase(0, pos + 1);
                }
            };

            while (!stop_token.stop_requested())
            {
                auto [events, ec] = process.poll(reproc::event::out | reproc::event::err, reproc::milliseconds(50));

                if (ec == std::errc::timed_out)
                {
                    continue;
                }
                if (ec)
                {
                    break; // stream closed or process exited
                }

                if (events & reproc::event::out)
                {
                    uint8_t buf[4096];
                    auto [bytes, read_ec] = process.read(reproc::stream::out, buf, sizeof(buf));
                    if (!read_ec && bytes > 0)
                        flush_lines(stdout_buffer, buf, bytes, [this](const auto &msg)
                                    { logger->info(msg); });
                }
                if (events & reproc::event::err)
                {
                    uint8_t buf[4096];
                    auto [bytes, read_ec] = process.read(reproc::stream::err, buf, sizeof(buf));
                    if (!read_ec && bytes > 0)
                        flush_lines(stderr_buffer, buf, bytes, [this](const auto &msg)
                                    { logger->error(msg); });
                }
            }

            if (stop_token.stop_requested())
            {
                logger->info("Stop requested. Worker thread is terminating child process: {}", config.commandLine[0]);
                std::error_code kill_ec = process.kill();
                if (kill_ec)
                {
                    logger->error("Failed to kill process: {}, {}", kill_ec.message(), config.commandLine[0]);
                }
            }

            auto [status, wait_ec] = process.wait(reproc::infinite);
            logger->info("Process exited with status {}. {}", status, config.commandLine[0]);

            if (stop_token.stop_requested())
            {
                break;
            }

            if (!config.auto_restart)
            {
                if (config.kill_parent_on_child_exit)
                {
                    logger->warn("Child process {} exited unexpectedly. Terminating main process.", config.commandLine[0]);
                    SDL_Event quit_event;
                    quit_event.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quit_event);
                }
                break;
            }

            // throttle restarts
            logger->warn("Process {} exited unexpectedly. Auto-restarting in 1 second...", config.commandLine[0]);
            auto restart_delay = std::chrono::seconds(1);
            auto start_time = std::chrono::steady_clock::now();
            while (!stop_token.stop_requested() && (std::chrono::steady_clock::now() - start_time) < restart_delay) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } });

    return true;
}

void ProcessRunner::stop()
{
    logger->info("Stopping process... {}", config.commandLine[0]);

    worker_thread.request_stop();

    if (worker_thread.joinable())
    {
        worker_thread.join();
    }

    logger->info("Process stopped successfully. {}", config.commandLine[0]);
}