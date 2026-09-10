#pragma once
#include "HandlerCommon.h"

// File provider owns this history; process provider never synthesizes file I/O.
void RecordFileEvent(uint16_t eventId, krabs::parser& parser, uint32_t processId);
