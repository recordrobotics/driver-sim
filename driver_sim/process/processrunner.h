#pragma once

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>
#include <tiny-process-library/process.hpp>

class ProcessRunner
{
public:
    struct Config
    {
        std::vector<std::string> commandLine;
        std::string working_directory = "";
        TinyProcessLib::Process::environment_type environment = {};
        bool kill_parent_on_child_exit = false;
        bool auto_restart = false;
    };

    ProcessRunner(Config config, std::shared_ptr<spdlog::logger> logger)
        : config(std::move(config)), logger(std::move(logger)) {}

    ~ProcessRunner()
    {
        stop();
    }

    ProcessRunner(const ProcessRunner &) = delete;
    ProcessRunner &operator=(const ProcessRunner &) = delete;
    ProcessRunner(ProcessRunner &&) noexcept = default;
    ProcessRunner &operator=(ProcessRunner &&) noexcept = default;

    bool start();

    void stop();

private:
    Config config;
    std::shared_ptr<spdlog::logger> logger;
    TinyProcessLib::Process::environment_type env;
    std::jthread worker_thread;
};