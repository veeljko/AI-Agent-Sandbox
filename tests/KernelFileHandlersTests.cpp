#include "../KernelFileProvider/ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "../KernelFileProvider/ProviderEventsHandlers/Common/HandlerCommon.h"
#include "../krabs/krabs/testing/record_builder.hpp"
#include <cassert>
#include <iostream>

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
    std::wcout << L"Kernel File handler tests passed (versions 0/1, completion policy, close cleanup).\n";
}
