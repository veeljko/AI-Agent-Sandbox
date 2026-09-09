#include "ProcessContext.h"
#include "../StartProcess/StartProcess.h"

#include <utility>

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
