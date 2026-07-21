#include "scan_history.h"

#include <Preferences.h>
#include <cstddef>

namespace {
constexpr uint32_t recordMagic = 0x53434E31UL;  // SCN1
constexpr char historyNamespace[] = "scanHistory";
}

uint32_t ScanHistoryStore::checksum(const ScanHistoryRecord& record) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    const size_t length = offsetof(ScanHistoryRecord, checksum);
    uint32_t value = 2166136261UL;
    for (size_t index = 0; index < length; ++index) {
        value ^= bytes[index];
        value *= 16777619UL;
    }
    return value;
}

String ScanHistoryStore::recordKey(uint8_t index) {
    char key[5];
    snprintf(key, sizeof(key), "r%02u", index);
    return String(key);
}

void ScanHistoryStore::begin() {
    Preferences preferences;
    if (!preferences.begin(historyNamespace, true)) return;
    count_ = preferences.getUChar("count", 0);
    if (count_ > capacity) count_ = capacity;
    next_ = preferences.getUChar("next", 0) % capacity;
    sequence_ = preferences.getUInt("sequence", 0);
    preferences.end();
}

bool ScanHistoryStore::append(ScanHistoryRecord record) {
    Preferences preferences;
    if (!preferences.begin(historyNamespace, false)) return false;
    record.magic = recordMagic;
    record.sequence = ++sequence_;
    record.checksum = checksum(record);
    const String key = recordKey(next_);
    const bool written = preferences.putBytes(key.c_str(), &record, sizeof(record)) == sizeof(record);
    if (written) {
        next_ = (next_ + 1) % capacity;
        if (count_ < capacity) ++count_;
        preferences.putUChar("count", count_);
        preferences.putUChar("next", next_);
        preferences.putUInt("sequence", sequence_);
    } else {
        --sequence_;
    }
    preferences.end();
    return written;
}

bool ScanHistoryStore::newest(uint8_t index, ScanHistoryRecord& record) const {
    if (index >= count_) return false;
    const uint8_t physicalIndex = (next_ + capacity - 1 - index) % capacity;
    Preferences preferences;
    if (!preferences.begin(historyNamespace, true)) return false;
    const String key = recordKey(physicalIndex);
    const bool read = preferences.getBytesLength(key.c_str()) == sizeof(record) &&
        preferences.getBytes(key.c_str(), &record, sizeof(record)) == sizeof(record);
    preferences.end();
    return read && record.magic == recordMagic && record.checksum == checksum(record);
}
