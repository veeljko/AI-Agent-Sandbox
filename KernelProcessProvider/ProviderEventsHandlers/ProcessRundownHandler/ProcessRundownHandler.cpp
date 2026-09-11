#include "ProcessRundownHandler.h"

void ProcessRundownHandler(krabs::parser& parser, HandlerContext& context) {
    RegisterProcessEvent(parser, context, L"PROCESS RUNDOWN");
}
