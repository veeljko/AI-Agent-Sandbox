#include "ProcessStartHandler.h"

void ProcessStartHandler(krabs::parser& parser, HandlerContext& context) {
    RegisterProcessEvent(parser, context, L"PROCESS START");
}
