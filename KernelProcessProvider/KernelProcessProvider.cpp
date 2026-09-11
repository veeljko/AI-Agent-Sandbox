#include "KernelProcessProvider.h"
#include "ProviderEventsHandlers/ProviderEventsHandlers.h"
#include <iostream>
#include <utility>

void KernelProcessProvider::ConfigureEvents() {
    // No Job I/O, priority or silo events: this provider owns lifecycle and modules only.
    provider_.any(0x10 | 0x20 | 0x40);
    auto callback = [this](const EVENT_RECORD& record, const krabs::trace_context& traceContext) {
        try {
            krabs::schema schema(record, traceContext.schema_locator);
            krabs::parser parser(schema);
            // Each handler reads payload ProcessID. Header PID can be the creator/system process.
            switch (schema.event_id()) {
                case 1: ProcessStartHandler(parser, context_); break;
                case 2: ProcessStopHandler(parser, context_); break;
                // case 3: ThreadStartHandler(parser, context_); break;
                // case 4: ThreadStopHandler(parser, context_); break;
                case 5: ImageLoadHandler(parser, context_); break;
                case 6: ImageUnloadHandler(parser, context_); break;
                case 15: ProcessRundownHandler(parser, context_); break;
                default: break;
            }
        } catch (const std::exception& error) {
            std::cerr << "KernelProcess callback error: " << error.what() << std::endl;
        }
    };
    provider_.add_on_event_callback(std::move(callback));
}
bool KernelProcessProvider::BootstrapRoot() { return BootstrapRootProcess(context_); }
void KernelProcessProvider::Enable(krabs::user_trace& trace) { trace.enable(provider_); }
