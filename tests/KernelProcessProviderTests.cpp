#include "../KernelProcessProvider/KernelProcessProvider.h"
#include "../KernelFileProvider/KernelFileProvider.h"
#include "../krabs/krabs/testing/record_builder.hpp"
#include "../krabs/krabs/testing/proxy.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

// Deterministic job membership boundary. These tests do not launch external processes.
const wchar_t workingDir[] = L"C:\\workspace";
namespace { int jobToken = 0; }
bool IsProcessInJob(HANDLE job, DWORD pid) { return job == &jobToken && (pid == 42 || pid == 43); }

int main() {
    ManagedJobProcess managed;
    managed.job = &jobToken;
    managed.processId = 42;
    auto& store = GetProcessContextStore();
    KernelProcessProvider processProvider(managed, store);
    std::atomic_bool alert = false;
    KernelFileProvider fileProvider(managed, alert);
    krabs::user_trace trace(L"ProviderResponsibilityTest");
    processProvider.Enable(trace);
    fileProvider.Enable(trace);
    krabs::testing::user_trace_proxy proxy(trace);
    const krabs::guid processGuid(L"{22fb2cd6-0e7b-422b-a0c7-2fad1fd0e716}");
    const krabs::guid fileGuid(L"{edd08927-9cc4-4e65-b970-c2560fb5c289}");
    FILETIME created{};
    GetSystemTimeAsFileTime(&created);
    auto ticks = FileTimeTicks(created);
    SID integrity{};
    integrity.Revision = SID_REVISION;
    integrity.SubAuthorityCount = 1;
    integrity.IdentifierAuthority.Value[5] = 16;
    integrity.SubAuthority[0] = 8192;
    std::ostringstream errors;
    auto* oldErrors = std::cerr.rdbuf(errors.rdbuf());

    // All supplied Start and Rundown versions deduplicate the same lifetime.
    for (size_t id : {size_t{1}, size_t{15}}) {
        const size_t lastVersion = id == 1 ? 4 : 2;
        for (size_t version = 0; version <= lastVersion; ++version) {
            krabs::testing::record_builder builder(processGuid, id, version, id == 1 ? 1 : 0);
            builder.header().ProcessId = 999; // The event header is not the target PID.
            builder.add_properties()(L"ProcessID", uint32_t{42})(L"CreateTime", created)
                (L"ParentProcessID", uint32_t{7})(L"ImageName", std::wstring(L"C:\\workspace\\child.exe"));
            if ((id == 1 && version >= 3) || (id == 15 && version >= 1)) {
                builder.add_properties()(L"ProcessSequenceNumber", uint64_t{900})(L"MandatoryLabel", integrity);
            }
            proxy.push_event(builder.pack_incomplete());
        }
    }
    assert(errors.str().empty());
    assert(store.Snapshot().size() == 1);
    auto key = store.FindProcessKey(42, ticks);
    assert(key);
    auto process = store.Find(*key);
    assert(process->parentPid == 7 && process->processSequenceNumber == 900);
    assert(process->integritySid == L"S-1-16-8192");
    assert(process->userSid.empty()); // Integrity SID must not be stored as user SID.
    assert(process->commandLine.empty()); // No command line field exists in this manifest.
    assert(process->images.empty() && process->files.empty());

    krabs::testing::record_builder unrelated(processGuid, 1, 0, 1);
    unrelated.header().ProcessId = 42;
    unrelated.add_properties()(L"ProcessID", uint32_t{999})(L"CreateTime", created);
    proxy.push_event(unrelated.pack_incomplete());
    assert(store.Snapshot().size() == 1);

    for (size_t version = 0; version <= 1; ++version) {
        for (size_t id : {size_t{3}, size_t{4}}) {
            krabs::testing::record_builder builder(processGuid, id, version, id == 3 ? 1 : 2);
            builder.add_properties()(L"ProcessID", uint32_t{42})(L"ThreadID", static_cast<uint32_t>(100 + version))
                (L"Win32StartAddr", reinterpret_cast<void*>(0x1000));
            proxy.push_event(builder.pack_incomplete());
        }
    }
    process = store.Find(*key);
    // Thread callbacks are intentionally disabled in KernelProcessProvider.
    assert(process->threads.empty());

    krabs::testing::record_builder image(processGuid, 5, 0);
    image.add_properties()(L"ProcessID", uint32_t{42})
        (L"ImageBase", reinterpret_cast<void*>(0x5000))(L"ImageSize", reinterpret_cast<void*>(4096))
        (L"ImageName", std::wstring(L"C:\\Windows\\System32\\example.dll"));
    auto imageEvent = image.pack_incomplete();
    proxy.push_event(imageEvent);
    proxy.push_event(imageEvent); // No duplicate module record.
    process = store.Find(*key);
    assert(process->images.size() == 1 && process->images[0].loaded);
    assert(process->files.empty()); // Module loading does not fabricate a file read.

    krabs::testing::record_builder open(fileGuid, 12, 1);
    open.header().ProcessId = 42;
    open.add_properties()(L"FileName", std::wstring(L"C:\\Windows\\System32\\example.dll"))
        (L"FileObject", reinterpret_cast<void*>(0x6000));
    proxy.push_event(open.pack_incomplete());
    krabs::testing::record_builder read(fileGuid, 15, 1);
    read.header().ProcessId = 42;
    read.add_properties()(L"FileObject", reinterpret_cast<void*>(0x6000))
        (L"ByteOffset", uint64_t{512})(L"IOSize", uint32_t{128});
    proxy.push_event(read.pack_incomplete());
    process = store.Find(*key);
    assert(process->images.size() == 1);
    assert(process->files.size() == 2 && process->files[1].ioSize == 128);
    assert(process->files[1].path.find(L"example.dll") != std::wstring::npos);

    krabs::testing::record_builder unload(processGuid, 6, 0);
    unload.add_properties()(L"ProcessID", uint32_t{42})(L"ImageBase", reinterpret_cast<void*>(0x5000));
    proxy.push_event(unload.pack_incomplete());
    assert(!store.Find(*key)->images[0].loaded);

    // All stop versions target the original lifetime; then simulate PID reuse.
    for (size_t version = 0; version <= 2; ++version) {
        krabs::testing::record_builder stop(processGuid, 2, version, 2);
        stop.add_properties()(L"ProcessID", uint32_t{42})(L"CreateTime", created)
            (L"ExitTime", created)(L"ExitCode", uint32_t{5});
        proxy.push_event(stop.pack_incomplete());
    }
    assert(!store.Find(*key)->running && store.Find(*key)->exitCode == 5);
    FILETIME nextCreated{static_cast<DWORD>(ticks + 10000000), static_cast<DWORD>((ticks + 10000000) >> 32)};
    krabs::testing::record_builder reused(processGuid, 1, 0, 1);
    reused.add_properties()(L"ProcessID", uint32_t{42})(L"CreateTime", nextCreated);
    proxy.push_event(reused.pack_incomplete());
    auto newKey = store.FindProcessKey(42, ticks + 10000000);
    assert(newKey && *newKey != *key);
    krabs::testing::record_builder lateStop(processGuid, 2, 0, 2);
    lateStop.add_properties()(L"ProcessID", uint32_t{42})(L"CreateTime", created);
    proxy.push_event(lateStop.pack_incomplete());
    assert(store.Find(*newKey)->running);
    std::cerr.rdbuf(oldErrors);
    if (!errors.str().empty()) std::cerr << errors.str();
    assert(errors.str().empty());
    std::cout << "Process provider and file/process separation tests passed.\n";
}
