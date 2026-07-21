#pragma once

#include <Arduino.h>

struct ScanHistoryRecord {
    uint32_t magic = 0;
    uint32_t sequence = 0;
    uint32_t durationMs = 0;
    uint32_t totalImages = 0;
    float startX = 0.0F;
    float startY = 0.0F;
    float startZ = 0.0F;
    float endX = 0.0F;
    float endY = 0.0F;
    float endZ = 0.0F;
    float frameX = 0.0F;
    float frameY = 0.0F;
    float stepX = 0.0F;
    float stepY = 0.0F;
    float stepZ = 0.0F;
    float actualOverlapX = 0.0F;
    float actualOverlapY = 0.0F;
    int16_t overlapMin = 0;
    int16_t overlapMax = 0;
    int16_t columns = 0;
    int16_t rows = 0;
    int16_t focusSteps = 0;
    int16_t speed = 0;
    int16_t zSpeed = 0;
    int16_t settleMs = 0;
    int16_t cameraPulseMs = 0;
    int16_t cameraRecoveryMs = 0;
    uint16_t cameraWidth = 0;
    uint16_t cameraHeight = 0;
    uint8_t flags = 0;
    uint8_t reserved[3] = {0, 0, 0};
    uint32_t checksum = 0;
};

static_assert(sizeof(ScanHistoryRecord) <= 128, "Scan history record must remain compact");

class ScanHistoryStore {
public:
    static constexpr uint8_t capacity = 32;
    static constexpr uint8_t cameraEnabledFlag = 1 << 0;
    static constexpr uint8_t returnToStartFlag = 1 << 1;
    static constexpr uint8_t uniformXFlag = 1 << 2;
    static constexpr uint8_t uniformYFlag = 1 << 3;
    static constexpr uint8_t timingMarkersFlag = 1 << 4;

    void begin();
    bool append(ScanHistoryRecord record);
    bool newest(uint8_t index, ScanHistoryRecord& record) const;
    uint8_t count() const { return count_; }
    uint32_t nextSequence() const { return sequence_ + 1; }

private:
    uint8_t count_ = 0;
    uint8_t next_ = 0;
    uint32_t sequence_ = 0;

    static uint32_t checksum(const ScanHistoryRecord& record);
    static String recordKey(uint8_t index);
};
