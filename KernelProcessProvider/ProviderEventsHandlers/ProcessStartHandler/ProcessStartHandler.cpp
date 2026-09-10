#include "ProcessStartHandler.h"

namespace KernelProcessHandlers {
void ProcessStartHandler(krabs::parser& parser, HandlerContext& context) {
    RegisterProcessEvent(parser, context, L"PROCESS START");
}
}
