#include "processrunner.h"
#include <filesystem>
#include <optional>
#include <string_view>
#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#elif defined(__linux__)
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#elif defined(__APPLE__)
#include <libproc.h>
#include <unistd.h>
#include <vector>
#endif

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

struct ExistingProcess
{
    int64_t pid;
};

std::optional<ExistingProcess> find_existing_process(const std::string_view executable_name)
{
    const auto wanted =
        std::filesystem::path(executable_name).filename().string();

#ifdef _WIN32

    const DWORD self = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return std::nullopt;

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (!Process32First(snapshot, &entry))
    {
        CloseHandle(snapshot);
        return std::nullopt;
    }

    do
    {
        if (entry.th32ProcessID == self)
            continue;

        if (wanted == entry.szExeFile)
        {
            CloseHandle(snapshot);
            return ExistingProcess{static_cast<int64_t>(entry.th32ProcessID)};
        }

    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);

#elif defined(__linux__)

    const pid_t self = getpid();

    DIR *dir = opendir("/proc");
    if (!dir)
        return std::nullopt;

    while (auto *ent = readdir(dir))
    {
        if (!std::isdigit(ent->d_name[0]))
            continue;

        pid_t pid = static_cast<pid_t>(std::atoi(ent->d_name));

        if (pid == self)
            continue;

        std::ifstream comm(std::string("/proc/") + ent->d_name + "/comm");

        std::string name;
        if (!std::getline(comm, name))
            continue;

        if (!name.empty() && name.back() == '\n')
            name.pop_back();

        if (name == wanted)
        {
            closedir(dir);
            return ExistingProcess{pid};
        }
    }

    closedir(dir);

#elif defined(__APPLE__)

    const pid_t self = getpid();

    int count = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (count <= 0)
        return std::nullopt;

    std::vector<pid_t> pids(count);

    count = proc_listpids(PROC_ALL_PIDS,
                          0,
                          pids.data(),
                          static_cast<int>(pids.size() * sizeof(pid_t)));

    count /= sizeof(pid_t);

    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];

    for (int i = 0; i < count; ++i)
    {
        pid_t pid = pids[i];

        if (pid == 0 || pid == self)
            continue;

        if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) <= 0)
            continue;

        if (std::filesystem::path(pathbuf).filename() == wanted)
            return ExistingProcess{pid};
    }

#else
#error Unsupported platform
#endif

    return std::nullopt;
}

void handle_existing_process(std::stop_token stop_token, const std::string_view executable_name, std::shared_ptr<spdlog::logger> logger)
{
    logger->info("Searching for existing process: {}", executable_name);

    auto existing = find_existing_process(executable_name);
    if (!existing)
    {
        logger->info("No existing process found for {}", executable_name);
        return;
    }

    logger->info("Found existing process: {}:{}", executable_name, existing.value().pid);

    while(!stop_token.stop_requested())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool ProcessRunner::start()
{
    env = make_inherited_env(config.environment);

    worker_thread = std::jthread([this](std::stop_token stop_token)
                                 {
        bool is_restart = false;

        while (!stop_token.stop_requested())
        {
            if (config.use_existing_process && !is_restart)
            {
                handle_existing_process(stop_token, config.commandLine[0], logger);
            }

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