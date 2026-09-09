#include "KernelFileProvider.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include "../StartProcess/StartProcess.h"

#include <exception>
#include <iostream>
#include <utility>

KernelFileProvider::KernelFileProvider(
    ManagedJobProcess& managedProcess,
    std::atomic_bool& sandboxReexecutionRequested
) : provider_(L"Microsoft-Windows-Kernel-File") {
    provider_.any(
        0x10  | // FileKey/name lifetime notifications
        0x100 | // Read
        0x200 | // Write
        0x20  | // File I/O
        0x40  | // OperationEnd (correlates Create/Open status)
        0x80  | // Create/Open
        0x400 | // DeletePath
        0x800 | // RenamePath / SetLinkPath
        0x1000  // CreateNewFile
    );
    
    auto eventManager = [&managedProcess, &sandboxReexecutionRequested](
        const EVENT_RECORD& record,
        const krabs::trace_context& trace_context
    ) {
        try {
            krabs::schema schema(record, trace_context.schema_locator);
            krabs::parser parser(schema);

            uint32_t processId = (record.EventHeader.ProcessId);
            // Name events describe shared file keys; their PID need not belong to our job.
            // Use them only for path correlation. Actual I/O remains filtered by job.
            if (schema.event_id() == 10) {
                NameCreateHandler(parser, processId);
                return;
            }
            if (schema.event_id() == 11) {
                NameDeleteHandler(parser, processId);
                return;
            }
            bool isValid = true;
            if (schema.event_id() == 24) {
                // The original handler correlates only protected creates captured from this job.
                isValid = OperationEndHandler(parser);
            } else {
                if (!IsProcessInJob(managedProcess.job, processId)) {
                    return;
                }
                switch (schema.event_id()) {
                case 12: isValid = CreateOpenHandler(parser, processId); break;
                case 14: isValid = CloseHandler(parser, processId); break;
                case 15: isValid = ReadHandler(parser, processId); break;
                case 16: isValid = WriteHandler(parser, processId); break;
                case 17: isValid = SetInformationHandler(parser, processId); break;
                case 18: isValid = SetDeleteHandler(parser, processId); break;
                case 19:
                case 29: isValid = RenameHandler(parser, processId); break;
                case 26: isValid = DeletePathHandler(parser, processId); break;
                case 27: isValid = RenamePathHandler(parser, processId); break;
                case 30: isValid = CreateNewFileHandler(parser, processId); break;
                default: break;
                }
            }
            if (!isValid && !sandboxReexecutionRequested.exchange(true)) {
                std::wcout << L"[ALERT] Proces pristupa folderu van radnog direktorijuma!" << std::endl;
                std::wcout << L"Gasim trenutni JobObject i prelazim na HCS sandbox." << std::endl;
                //TerminateManagedJob(managedProcess, 1);
            }

        } catch (const std::exception& ex) {
            std::cerr << "Callback error: " << ex.what() << std::endl;
        } catch (...) {
            std::cerr << "Callback error: unknown exception" << std::endl;
        }
    };

    // krabs stores non-const lvalues by reference. This local lambda must be
    // copied into the provider before the constructor returns.
    provider_.add_on_event_callback(std::move(eventManager));
}

void KernelFileProvider::Enable(krabs::user_trace& trace) {
    trace.enable(provider_);
}
