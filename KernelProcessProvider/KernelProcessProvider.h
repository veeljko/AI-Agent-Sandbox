#pragma once
#include "../krabs/krabs.hpp"
#include "ProviderEventsHandlers/Common/ProcessHandlerCommon.h"

class KernelProcessProvider {
public:
    // The managed job and store outlive the ETW trace callbacks.
    explicit KernelProcessProvider(ManagedJobProcess& managedProcess,
        ProcessContextStore& store = GetProcessContextStore());
    bool BootstrapRoot();
    void Enable(krabs::user_trace& trace);
private:
    krabs::provider<> provider_;
    KernelProcessHandlers::HandlerContext context_;
};
