#include "../ProcessContext/ProcessContext.h"
#include "../StartProcess/StartProcess.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
    int jobToken = 0;
    HANDLE testJob = &jobToken;
}

// Test double for the existing job-membership check; only PID 42 belongs to this job.
bool IsProcessInJob(HANDLE job, DWORD processId) {
    return job == testJob && processId == 42;
}

int main() {
    ProcessContextStore store;
    ManagedJobProcess managedProcess;
    managedProcess.job = testJob;
    ProcessContext first;
    first.pid = 42;
    first.uniqueProcessKey = 100;
    assert(store.Add(managedProcess, first));
    assert(!store.Add(managedProcess, first));
    assert(!store.Add(managedProcess, ProcessContext{}));

    ProcessContext outsider = first;
    outsider.pid = 99;
    outsider.uniqueProcessKey = 300;
    assert(!store.Add(managedProcess, outsider));
    assert(!store.Find(300));

    ManagedJobProcess missingJob;
    ProcessContext candidate = first;
    candidate.uniqueProcessKey = 400;
    assert(!store.Add(missingJob, candidate));
    missingJob.job = INVALID_HANDLE_VALUE;
    assert(!store.Add(missingJob, candidate));
    int otherJobToken = 0;
    missingJob.job = &otherJobToken;
    assert(!store.Add(missingJob, candidate));
    assert(!store.Find(400));

    // PID reuse must preserve both process lifetimes.
    ProcessContext second = first;
    second.uniqueProcessKey = 200;
    assert(store.Add(managedProcess, second));
    assert(store.Snapshot().size() == 2);
    assert(!store.Find(300));
    assert(!store.Update(300, [](ProcessContext&) {}));

    // Readers receive independent copies, not mutable references to shared data.
    auto copy = store.Find(100);
    copy->image = L"changed";
    assert(store.Find(100)->image.empty());
    assert(!store.Update(100, [](ProcessContext& context) { context.pid = 99; }));
    assert(!store.Update(100, [](ProcessContext& context) { context.uniqueProcessKey = 999; }));
    try {
        store.Update(100, [](ProcessContext& context) {
            context.threatScore = 99;
            throw std::runtime_error("test");
        });
        assert(false);
    } catch (const std::runtime_error&) {
        assert(store.Find(100)->threatScore == 0);
    }

    // Concurrent read-modify-write operations must not lose events or increments.
    std::vector<std::thread> writers;
    for (int worker = 0; worker < 4; ++worker) {
        writers.emplace_back([&store]() {
            for (int index = 0; index < 100; ++index) {
                assert(store.Update(100, [](ProcessContext& context) {
                    ++context.threatScore;
                    context.files.push_back(FileEvent{});
                }));
            }
        });
    }
    for (auto& writer : writers) {
        writer.join();
    }
    assert(store.Find(100)->threatScore == 400);
    assert(store.Find(100)->files.size() == 400);
    assert(store.Find(200)->threatScore == 0);
    assert(store.Remove(100));
    assert(!store.Find(100));
    assert(store.Find(200));
    assert(&GetProcessContextStore() == &GetProcessContextStore());
    std::cout << "ProcessContext tests passed.\n";
}
