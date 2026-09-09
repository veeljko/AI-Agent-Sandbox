#pragma once

#include "../krabs/krabs.hpp"
#include <atomic>

struct ManagedJobProcess;

class KernelFileProvider {
public:
    // Both arguments must remain alive while the trace processes events.
    KernelFileProvider(
        ManagedJobProcess& managedProcess,
        std::atomic_bool& sandboxReexecutionRequested
    );

    void Enable(krabs::user_trace& trace);

private:
    krabs::provider<> provider_;
};
