#include "ProcessContext.h"
#include "../StartProcess/StartProcess.h"

#include <utility>

std::optional<ProcessContextStore::Registration> ProcessContextStore::Register(
    const ManagedJobProcess& managedProcess, ProcessContext context
) {
    if (!context.pid || !context.createTimeTicks || !managedProcess.job ||
        managedProcess.job == INVALID_HANDLE_VALUE ||
        !IsProcessInJob(managedProcess.job, context.pid)) {
        return std::nullopt;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : contexts_) {
        auto& existing = entry.second;
        if (existing.pid == context.pid && existing.createTimeTicks == context.createTimeTicks) {
            if (context.parentPid) existing.parentPid = context.parentPid;
            if (context.processSequenceNumber) existing.processSequenceNumber = context.processSequenceNumber;
            if (!context.image.empty()) existing.image = std::move(context.image);
            if (!context.commandLine.empty()) existing.commandLine = std::move(context.commandLine);
            if (!context.userSid.empty()) existing.userSid = std::move(context.userSid);
            if (!context.integritySid.empty()) existing.integritySid = std::move(context.integritySid);
            return Registration{entry.first, false};
        }
    }
    while (!nextKey_ || contexts_.count(nextKey_)) ++nextKey_;
    const uint64_t key = nextKey_++;
    context.uniqueProcessKey = key;
    contexts_.emplace(key, std::move(context));
    return Registration{key, true};
}

std::optional<uint64_t> ProcessContextStore::FindProcessKey(uint32_t pid, uint64_t createTimeTicks) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const ProcessContext* newest = nullptr;
    for (const auto& entry : contexts_) {
        const auto& context = entry.second;
        if (context.pid != pid) continue;
        if (createTimeTicks && context.createTimeTicks != createTimeTicks) continue;
        if (!newest || context.createTimeTicks > newest->createTimeTicks) newest = &context;
    }
    return newest ? std::optional<uint64_t>(newest->uniqueProcessKey) : std::nullopt;
}

bool ProcessContextStore::Add(const ManagedJobProcess& managedProcess, ProcessContext context) {
    if (context.pid == 0 || context.uniqueProcessKey == 0 ||
        managedProcess.job == nullptr || managedProcess.job == INVALID_HANDLE_VALUE) {
        return false;
    }
    if (!IsProcessInJob(managedProcess.job, context.pid)) {
        return false;
    }

    const uint64_t key = context.uniqueProcessKey;
    std::lock_guard<std::mutex> lock(mutex_);
    return contexts_.emplace(key, std::move(context)).second;
}

std::optional<ProcessContext> ProcessContextStore::Find(uint64_t uniqueProcessKey) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = contexts_.find(uniqueProcessKey);
    if (found == contexts_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<ProcessContext> ProcessContextStore::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessContext> result;
    result.reserve(contexts_.size());
    for (const auto& entry : contexts_) {
        result.push_back(entry.second);
    }
    return result;
}

bool ProcessContextStore::Update(
    uint64_t uniqueProcessKey,
    const std::function<void(ProcessContext&)>& update
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = contexts_.find(uniqueProcessKey);
    if (found == contexts_.end() || !update) {
        return false;
    }

    ProcessContext updated = found->second;
    update(updated);
    if (updated.pid != found->second.pid || updated.uniqueProcessKey != uniqueProcessKey) {
        return false;
    }
    found->second = std::move(updated);
    return true;
}

bool ProcessContextStore::Remove(uint64_t uniqueProcessKey) {
    std::lock_guard<std::mutex> lock(mutex_);
    return contexts_.erase(uniqueProcessKey) != 0;
}

ProcessContextStore& GetProcessContextStore() {
    static ProcessContextStore store;
    return store;
}
