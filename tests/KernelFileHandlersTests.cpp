#include "../KernelFileProvider/ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "../KernelFileProvider/ProviderEventsHandlers/Common/HandlerCommon.h"
#include "../KernelFileProvider/KernelFileProvider.h"
#include "../StartProcess/StartProcess.h"
#include "../krabs/krabs/testing/record_builder.hpp"
#include "../krabs/krabs/testing/proxy.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

__declspec(noinline) void OverwriteUnusedStack() {
    volatile unsigned char bytes[16384];
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        bytes[index] = 0;
    }
}

using Handler = bool (*)(krabs::parser&, uint32_t);
int main() {
    const krabs::guid provider(L"{edd08927-9cc4-4e65-b970-c2560fb5c289}");
    krabs::schema_locator locator;
    struct Case { size_t id; Handler handler; };
    const Case cases[] = {
        {12, CreateOpenHandler}, {14, CloseHandler}, {15, ReadHandler},
        {16, WriteHandler}, {19, RenameHandler}, {26, DeletePathHandler},
        {27, RenamePathHandler}, {29, RenameHandler}, {30, CreateNewFileHandler}
    };
    for (const auto& test : cases) {
        for (size_t version = 0; version <= 1; ++version) {
            krabs::testing::record_builder builder(provider, test.id, version);
            auto event = builder.pack_incomplete();
            krabs::schema schema(event, locator);
            krabs::parser parser(schema);
            assert(test.handler(parser, 42));
        }
    }
    // Directory opens must populate the map even though they bypass file policy.
    krabs::testing::record_builder directoryBuilder(provider, 12, 1);
    directoryBuilder.add_properties()
        (L"FileObject", reinterpret_cast<void*>(0xA001))
        (L"CreateOptions", uint32_t{1})
        (L"FileName", std::wstring(L"C:\\Windows"));
    auto directoryEvent = directoryBuilder.pack_incomplete();
    krabs::schema directorySchema(directoryEvent, locator);
    krabs::parser directoryParser(directorySchema);
    assert(CreateOpenHandler(directoryParser, 42));
    assert(!FileHandlerCommon::file_object_to_path.at(0xA001).empty());

    // A file-name notification can resolve multiple distinct FileObjects via FileKey.
    krabs::testing::record_builder nameBuilder(provider, 10, 0);
    nameBuilder.add_properties()
        (L"FileKey", reinterpret_cast<void*>(0xB001))
        (L"FileName", std::wstring(L"C:\\Windows\\System32\\Wldp.dll"));
    auto nameEvent = nameBuilder.pack_incomplete();
    krabs::schema nameSchema(nameEvent, locator);
    krabs::parser nameParser(nameSchema);
    assert(NameCreateHandler(nameParser, 4));
    for (uintptr_t object : {uintptr_t{0xA002}, uintptr_t{0xA003}}) {
        krabs::testing::record_builder builder(provider, 14, 1);
        builder.add_properties()
            (L"FileObject", reinterpret_cast<void*>(object))
            (L"FileKey", reinterpret_cast<void*>(0xB001));
        auto event = builder.pack_incomplete();
        krabs::schema schema(event, locator);
        krabs::parser parser(schema);
        {
            std::lock_guard<std::mutex> lock(FileHandlerCommon::mutex);
            assert(FileHandlerCommon::ResolveFilePath(parser).find(L"Wldp.dll") != std::wstring::npos);
        }
        assert(CloseHandler(parser, 42));
        assert(FileHandlerCommon::file_key_to_path.count(0xB001) == 1);
    }
    krabs::testing::record_builder deleteNameBuilder(provider, 11, 0);
    deleteNameBuilder.add_properties()(L"FileKey", reinterpret_cast<void*>(0xB001));
    auto deleteNameEvent = deleteNameBuilder.pack_incomplete();
    krabs::schema deleteNameSchema(deleteNameEvent, locator);
    krabs::parser deleteNameParser(deleteNameSchema);
    assert(NameDeleteHandler(deleteNameParser, 4));
    assert(FileHandlerCommon::file_key_to_path.count(0xB001) == 0);

    const uint64_t irp = 0x1234;
    const uint64_t fileObject = 0x5678;
    auto complete = [&](uint32_t status) {
        krabs::testing::record_builder builder(provider, 24, 0);
        builder.add_properties()(L"Irp", reinterpret_cast<void*>(irp))(L"Status", status);
        auto event = builder.pack_incomplete();
        krabs::schema schema(event, locator);
        krabs::parser parser(schema);
        return OperationEndHandler(parser);
    };
    assert(complete(0)); // Untracked IRP must not trigger an alert.
    FileHandlerCommon::pending_protected_creates[irp] = {L"", fileObject, 42};
    assert(complete(0xC0000022)); // Access denied must not trigger an alert.
    assert(FileHandlerCommon::pending_protected_creates.empty());
    FileHandlerCommon::pending_protected_creates[irp] = {L"", fileObject, 42};
    assert(!complete(0));
    assert(FileHandlerCommon::pending_protected_creates.empty());
    assert(FileHandlerCommon::file_object_to_path.count(fileObject) == 1);
    krabs::testing::record_builder closeBuilder(provider, 14, 1);
    closeBuilder.add_properties()(L"FileObject", reinterpret_cast<void*>(fileObject));
    auto closeEvent = closeBuilder.pack_incomplete();
    krabs::schema closeSchema(closeEvent, locator);
    krabs::parser closeParser(closeSchema);
    assert(CloseHandler(closeParser, 42));
    assert(FileHandlerCommon::file_object_to_path.count(fileObject) == 0);
    // Exercise the registered callback after its constructor-local lambda is gone.
    ManagedJobProcess managedProcess;
    managedProcess.job = CreateJobObjectW(nullptr, nullptr);
    assert(managedProcess.job != nullptr);
    std::atomic_bool requested = false;
    krabs::user_trace trace(L"KernelFileCallbackLifetimeTest");
    KernelFileProvider kernelProvider(managedProcess, requested);
    kernelProvider.Enable(trace);
    krabs::testing::user_trace_proxy proxy(trace);
    OverwriteUnusedStack();

    std::ostringstream errors;
    auto* previousErrors = std::cerr.rdbuf(errors.rdbuf());
    krabs::testing::record_builder readBuilder(provider, 15, 1);
    auto readEvent = readBuilder.pack_incomplete();
    proxy.push_event(readEvent); // Empty valid job: reject event without invalid-handle errors.
    std::cerr.rdbuf(previousErrors);
    assert(errors.str().empty());

    FileHandlerCommon::pending_protected_creates[irp] = {L"", fileObject, 42};
    krabs::testing::record_builder endBuilder(provider, 24, 0);
    endBuilder.add_properties()(L"Irp", reinterpret_cast<void*>(irp))(L"Status", uint32_t{0});
    auto endEvent = endBuilder.pack_incomplete();
    proxy.push_event(endEvent);
    assert(requested.load()); // The callback must still reference the original atomic flag.
    CloseManagedJobProcess(managedProcess);
    std::wcout << L"Kernel File handler tests passed (versions, policy, cleanup, callback lifetime).\n";
}
