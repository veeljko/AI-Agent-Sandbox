#pragma once

#include <cstdint>

inline bool IsDirectoryCreateOpenOptions(uint32_t rawCreateOptions) {
    constexpr uint32_t kFileDirectoryFile = 0x00000001;
    const uint32_t createOptions = rawCreateOptions & 0x00FFFFFF;
    return (createOptions & kFileDirectoryFile) != 0;
}

inline bool IsSuccessfulNtStatus(uint32_t status) {
    constexpr uint32_t kNtStatusSeverityErrorBit = 0x80000000;
    return (status & kNtStatusSeverityErrorBit) == 0;
}

inline bool IsCompletedFileOperationSuccessful(
    bool hasStatus,
    uint32_t status
) {
    return hasStatus && IsSuccessfulNtStatus(status);
}
