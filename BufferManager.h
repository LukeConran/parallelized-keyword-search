#pragma once

#include "pch.h"

class BufferManager {
private:
    char* buffer;              // Pointer to the entire allocated memory block
    int slotSize;              // Size of each slot including shadow regions
    int dataSize;              // Actual data size per slot (B bytes from command line)
    int shadowSize;            // Size of shadow buffer for boundary strings
    int numSlots;              // Number of slots (N from command line)
    DWORD sectorSize;          // System sector size for alignment
    BOOL nonBufferedIO;        // Whether to use non-buffered I/O

public:
    BufferManager(int powerOfTwo, int slots, BOOL nonBuffered, int maxKeywordLength) {
        dataSize = 1 << powerOfTwo;
        numSlots = slots;
        nonBufferedIO = nonBuffered;

        GetDiskFreeSpace(NULL, NULL, &sectorSize, NULL, NULL);

        if (nonBufferedIO) {
            shadowSize = ((maxKeywordLength + sectorSize - 1) / sectorSize) * sectorSize;
        }
        else {
            shadowSize = maxKeywordLength;
        }
        int nullTerminatorBuffer = nonBufferedIO ? sectorSize : 1; // Only need 1 byte for null if buffered
        slotSize = dataSize + shadowSize + nullTerminatorBuffer;

        buffer = (char*)VirtualAlloc(NULL, (UINT64)numSlots * slotSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!buffer) {
            buffer = NULL;
            printf("Error: Failed to allocate buffer memory. Required size: %llu bytes\n", (UINT64)numSlots * slotSize);
        }

        if (buffer) {
            ZeroMemory(buffer, (UINT64)numSlots * slotSize);
        }
    }

    ~BufferManager() {
        if (buffer) {
            VirtualFree(buffer, 0, MEM_RELEASE);
            buffer = NULL;
        }
    }

    char* GetSlot(int slotID) {
        if (slotID < 0 || slotID >= numSlots) {
            return nullptr;
        }

        return buffer + (slotID * slotSize) + shadowSize;
    }

    char* GetShadowBuffer(int slotID) {
        if (slotID < 0 || slotID >= numSlots) {
            return nullptr;
        }

        return buffer + (slotID * slotSize);
    }

    void CopyShadowBuffer(int sourceSlotID, int destinationSlotID, int bytesToCopy) {
        if (sourceSlotID < 0 || sourceSlotID >= numSlots ||
            destinationSlotID < 0 || destinationSlotID >= numSlots) {
            return;
        }

        if (bytesToCopy > shadowSize) {
            bytesToCopy = shadowSize;
        }

        char* source = GetSlot(sourceSlotID) + (dataSize - bytesToCopy);
        char* destination = GetShadowBuffer(destinationSlotID);
        memcpy(destination, source, bytesToCopy);
    }

    int GetDataSize() const { return dataSize; }
    int GetSlotSize() const { return slotSize; }
    int GetShadowSize() const { return shadowSize; }
    int GetNumSlots() const { return numSlots; }
    DWORD GetSectorSize() const { return sectorSize; }
    bool IsNonBufferedIO() const { return nonBufferedIO; }
};