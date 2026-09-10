#include "KernelProcessProvider.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include <iostream>
#include <utility>

KernelProcessProvider::KernelProcessProvider(ManagedJobProcess& managedProcess, ProcessContextStore& store)
    : provider_(L"Microsoft-Windows-Kernel-Process"), context_{managedProcess, store} {
    // No Job I/O, priority or silo events: this provider owns lifecycle and modules only.
    provider_.any(0x10 | 0x20 | 0x40);
    auto callback = [this](const EVENT_RECORD& record, const krabs::trace_context& traceContext) {
        try {
            krabs::schema schema(record, traceContext.schema_locator);
            krabs::parser parser(schema);
            // Each handler reads payload ProcessID. Header PID can be the creator/system process.
            switch (schema.event_id()) {
            case 1: KernelProcessHandlers::ProcessStartHandler(parser, context_); break;
            case 2: KernelProcessHandlers::ProcessStopHandler(parser, context_); break;
            case 3: KernelProcessHandlers::ThreadStartHandler(parser, context_); break;
            case 4: KernelProcessHandlers::ThreadStopHandler(parser, context_); break;
            case 5: KernelProcessHandlers::ImageLoadHandler(parser, context_); break;
            case 6: KernelProcessHandlers::ImageUnloadHandler(parser, context_); break;
            case 15: KernelProcessHandlers::ProcessRundownHandler(parser, context_); break;
            default: break;
            }
        } catch (const std::exception& error) {
            std::cerr << "KernelProcess callback error: " << error.what() << std::endl;
        }
    };
    provider_.add_on_event_callback(std::move(callback));
}
bool KernelProcessProvider::BootstrapRoot() { return KernelProcessHandlers::BootstrapRoot(context_); }
void KernelProcessProvider::Enable(krabs::user_trace& trace) { trace.enable(provider_); }
