#pragma once
#include "../krabs/krabs.hpp"
#include "ProviderEventsHandlers/Common/ProcessHandlerCommon.h"

class KernelProcessProvider {
public:
    // The managed job and store outlive the ETW trace callbacks.
    KernelProcessProvider(ManagedJobProcess& managedProcess,
        ProcessContextStore& store = GetProcessContextStore())
        : provider_(L"Microsoft-Windows-Kernel-Process"), context_{managedProcess, store} {
        ConfigureEvents();
    }
    bool BootstrapRoot();
    void Enable(krabs::user_trace& trace);
private:
    void ConfigureEvents();
    krabs::provider<> provider_;
    HandlerContext context_;
};
