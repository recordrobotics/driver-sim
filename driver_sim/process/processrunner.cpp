#include "processrunner.h"

#include <SDL3/SDL.h>

using namespace TinyProcessLib;

#ifdef _WIN32
#include <cstdlib>
extern "C" char **_environ;
#define ENV_PTR _environ
#else
#include <unistd.h>
extern char **environ;
#define ENV_PTR environ
#endif

Process::environment_type make_inherited_env(
    const Process::environment_type &overrides)
{
    Process::environment_type env;

    // inherit parent environment
    for (char **e = ENV_PTR; e && *e; ++e)
    {
        std::string entry(*e);

        auto pos = entry.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = entry.substr(0, pos);
        std::string value = entry.substr(pos + 1);

        env[key] = value;
    }

    // append custom env
    for (const auto &kv : overrides)
    {
        env[kv.first] = kv.second;
    }

    return env;
}

bool ProcessRunner::start()
{
    env = make_inherited_env(config.environment);

    worker_thread = std::jthread([this](std::stop_token stop_token)
                                 {
        bool is_restart = false;

        while (!stop_token.stop_requested())
        {
            if (is_restart)
            {
                logger->info("Auto-restarting process: {}", config.commandLine[0]);
            }

            is_restart = true;
            restart_requested = false;

            std::mutex process_mutex;
            std::string stdout_buffer;
            std::string stderr_buffer;

            // aggregate stream chunks into complete lines
            auto flush_lines = [](std::string &buffer, const char *bytes, size_t size,
                                  const std::function<void(const std::string &)> &log_func)
            {
                buffer.append(bytes, size);
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

            Process process(
                config.commandLine,
                config.working_directory,
                env,
                [&](const char *bytes, size_t n)
                {
                    std::scoped_lock lock(process_mutex);

                    flush_lines(stdout_buffer, bytes, n,
                        [this](const std::string &msg)
                        {
                            logger->info(msg);
                        });
                },
                [&](const char *bytes, size_t n)
                {
                    std::scoped_lock lock(process_mutex);

                    flush_lines(stderr_buffer, bytes, n,
                        [this](const std::string &msg)
                        {
                            logger->error(msg);
                        });
                });

            if (process.get_id() <= 0)
            {
                logger->error("Failed to start process: {}", config.commandLine[0]);

                auto fail_delay = std::chrono::seconds(2);
                auto start_time = std::chrono::steady_clock::now();

                while (!stop_token.stop_requested() &&
                       std::chrono::steady_clock::now() - start_time < fail_delay)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                continue;
            }

            logger->info(
                "Process started successfully. PID: {}, {}",
                process.get_id(),
                config.commandLine[0]);


            int exit_status = -1;

            while (!stop_token.stop_requested() && !restart_requested)
            {
                if (process.try_get_exit_status(exit_status))
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            if (stop_token.stop_requested() || restart_requested)
            {
                logger->info("Stop requested. Worker thread is terminating child process: {}", config.commandLine[0]);
                process.kill(true);
            }

            exit_status = process.get_exit_status();

            logger->info(
                "Process exited with status {}. {}",
                exit_status,
                config.commandLine[0]);

            if (stop_token.stop_requested())
            {
                break;
            }

            if (!config.auto_restart && !restart_requested)
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

void ProcessRunner::restart()
{
    logger->info("Restarting process... {}", config.commandLine[0]);
    restart_requested = true;
}