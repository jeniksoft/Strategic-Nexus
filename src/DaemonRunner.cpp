#include "DaemonRunner.h"

#include "RequestFileReader.h"
#include "StrategicWorker.h"
#include "ModIntegration.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace strategic_nexus {

namespace {

struct ExchangePaths {
    std::filesystem::path requestPath;
    std::filesystem::path responsePath;
    std::filesystem::path lockPath;
    std::filesystem::path logPath;
};

ExchangePaths buildExchangePaths(const std::filesystem::path& exchangeDirectory)
{
    return {
        exchangeDirectory / "request.json",
        exchangeDirectory / "response.json",
        exchangeDirectory / "daemon.lock",
        exchangeDirectory / "daemon.log"
    };
}

void appendLog(const std::filesystem::path& logPath, const std::string& message)
{
    std::ofstream log(logPath, std::ios::app);
    if (!log) {
        std::cerr << "Unable to write daemon log file: " << logPath.string() << "\n";
        return;
    }

    log << message << "\n";
}

std::optional<std::filesystem::file_time_type> tryGetLastWriteTime(const std::filesystem::path& path)
{
    std::error_code error;
    const auto timestamp = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }

    return timestamp;
}

class LockFileGuard {
public:
    explicit LockFileGuard(std::filesystem::path lockPath)
        : lockPath_(std::move(lockPath))
    {
        acquire();
    }

    ~LockFileGuard()
    {
        std::error_code error;
        std::filesystem::remove(lockPath_, error);
    }

    void heartbeat() const
    {
        write("Strategic Nexus daemon active\n");
    }

private:
    void acquire() const
    {
        if (std::filesystem::exists(lockPath_)) {
            std::cout << "Existing daemon lock found and treated as stale/runtime marker: "
                << lockPath_.string() << "\n";
        }

        heartbeat();
    }

    void write(const std::string& content) const
    {
        std::ofstream lockFile(lockPath_, std::ios::trunc);
        if (!lockFile) {
            throw std::runtime_error("Unable to write daemon lock file: " + lockPath_.string());
        }

        lockFile << content;
    }

    std::filesystem::path lockPath_;
};

} // namespace

int DaemonRunner::run(const DaemonConfig& config) const
{
    std::filesystem::create_directories(config.exchangeDirectory);

    const ExchangePaths paths = buildExchangePaths(config.exchangeDirectory);
    const LockFileGuard lock(paths.lockPath);

    std::optional<std::filesystem::file_time_type> lastProcessedTimestamp;
    int iterations = 0;

    std::cout << "Strategic Nexus daemon watching: " << config.exchangeDirectory.string() << "\n";
    std::cout << "Request file: " << paths.requestPath.string() << "\n";
    appendLog(paths.logPath, "daemon started");

    while (config.maxIterations == 0 || iterations < config.maxIterations) {
        ++iterations;

        if (const auto requestTimestamp = tryGetLastWriteTime(paths.requestPath)) {
            if (!lastProcessedTimestamp || *requestTimestamp != *lastProcessedTimestamp) {
                appendLog(paths.logPath, "request detected: " + paths.requestPath.string());
                std::cout << "Processing request: " << paths.requestPath.string() << "\n";

                try {
                    StrategicRequest request = RequestFileReader().read(paths.requestPath);
                    request.outputDoctrinePath = paths.responsePath;

                    const int result = StrategicWorker().processRequestSafely(request);
                    appendLog(paths.logPath, result == 0
                        ? "request completed: " + request.requestId
                        : "request failed: " + request.requestId);
                }
                catch (const std::exception& error) {
                    StrategicRequest failedRequest;
                    failedRequest.requestId = "unreadable-request";
                    failedRequest.outputDoctrinePath = paths.responsePath;

                    ModIntegration().writeErrorJson(paths.responsePath, failedRequest, error.what());
                    appendLog(paths.logPath, std::string("request unreadable: ") + error.what());
                    std::cerr << "Unable to process request file: " << error.what() << "\n";
                }

                lastProcessedTimestamp = requestTimestamp;
            }
        }

        lock.heartbeat();
        std::this_thread::sleep_for(config.pollInterval);
    }

    appendLog(paths.logPath, "daemon stopped");
    return 0;
}

} // namespace strategic_nexus
