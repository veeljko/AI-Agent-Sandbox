#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using TimePoint = std::chrono::system_clock::time_point;

struct ManagedJobProcess;

// Initial event models; providers populate these when event collection is added.
struct LoadedImage {
    TimePoint timestamp{};
    std::wstring path;
    uint64_t baseAddress = 0;
    uint64_t size = 0;
    bool loaded = true;
    TimePoint unloadTime{};
};

struct ThreadContext {
    uint32_t tid = 0;
    uint64_t startAddress = 0;
    bool running = true;
};

struct FileEvent {
    TimePoint timestamp{};
    std::wstring path;
    std::wstring operation;
    uint16_t eventId = 0;
    std::optional<uint64_t> byteOffset;
    std::optional<uint32_t> ioSize;
};

struct RegistryEvent {
    TimePoint timestamp{};
    std::wstring keyPath;
    std::wstring valueName;
    std::wstring operation;
};

struct NetworkConnection {
    TimePoint timestamp{};
    std::wstring protocol;
    std::wstring localAddress;
    std::wstring remoteAddress;
    uint16_t localPort = 0;
    uint16_t remotePort = 0;
};

struct ProcessContext {
    uint32_t pid = 0;
    uint32_t parentPid = 0;
    uint64_t uniqueProcessKey = 0;

    std::wstring image;
    std::wstring commandLine;
    std::wstring userSid;
    TimePoint startTime{};
    // Kernel-Process identity. uniqueProcessKey is the store's own stable key;
    // ProcessSequenceNumber is optional in older manifest versions.
    uint64_t createTimeTicks = 0;
    uint64_t processSequenceNumber = 0;
    std::wstring integritySid;
    bool running = true;
    std::optional<uint32_t> exitCode;
    TimePoint exitTime{};

    std::vector<LoadedImage> images;
    std::vector<ThreadContext> threads;
    std::vector<FileEvent> files;
    std::vector<RegistryEvent> registry;
    std::vector<NetworkConnection> connections;

    int threatScore = 0;
};

// Thread-safe in-memory storage. Entries survive process exit until explicitly removed.
class ProcessContextStore {
public:
    // Requires membership in the managed job, plus nonzero PID and uniqueProcessKey.
    // Uses the same IsProcessInJob check as KernelFileProvider (including known exited PIDs).
    // Duplicates are not overwritten.
    bool Add(const ManagedJobProcess& managedProcess, ProcessContext context);
    struct Registration { uint64_t key; bool inserted; };
    // Deduplicates Start/Rundown/bootstrap by PID + native creation time.
    std::optional<Registration> Register(const ManagedJobProcess& managedProcess, ProcessContext context);
    std::optional<uint64_t> FindProcessKey(uint32_t pid, uint64_t createTimeTicks = 0) const;
    std::optional<ProcessContext> Find(uint64_t uniqueProcessKey) const;
    std::vector<ProcessContext> Snapshot() const;

    // Runs under the store lock: do not call this store from inside the callback.
    // Changes to PID/key are rejected. Failed/throwing callbacks leave the entry intact.
    bool Update(uint64_t uniqueProcessKey,
                const std::function<void(ProcessContext&)>& update);
    bool Remove(uint64_t uniqueProcessKey);

private:
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, ProcessContext> contexts_;
    uint64_t nextKey_ = 1;
};

// One shared store for the application, accessible by including this header.
ProcessContextStore& GetProcessContextStore();
