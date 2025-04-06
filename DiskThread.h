#pragma once

#include "pch.h"
#include "ProducerConsumer.h"
#include "BufferManager.h"
#include "MyBuf.h"

struct DiskThreadContext {
    const char* filename;           // File to search
    ProducerConsumer* pcEmpty;                    // Queue for empty slots
    ProducerConsumer* pcFull;                     // Queue for filled slots
    BufferManager* bufferManager;   // Buffer manager
    HANDLE eventQuit;               // Event to signal threads to quit
    BOOL nonBufferedIO;             // Whether to use non-buffered I/O
    int maxKeywordLength;           // Maximum keyword length for shadow buffer
};

// Disk reading thread function
DWORD WINAPI DiskReadThread(LPVOID param) {
    // Cast the parameter to our context structure
    DiskThreadContext* ctx = (DiskThreadContext*)param;

    // Open the file
    HANDLE hFile = CreateFile(
        ctx->filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        ctx->nonBufferedIO ? FILE_FLAG_NO_BUFFERING : FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        // Handle error
        SetEvent(ctx->eventQuit); // Signal all threads to quit
        return 1;
    }

    // Set thread priority higher than search threads
    SetThreadPriority(GetCurrentThread(), ABOVE_NORMAL_PRIORITY_CLASS);

    int prevSlotID = -1;     // Track the previous slot for shadow buffer copying
    BOOL reachedEOF = FALSE; // Flag for end of file

    // Main reading loop
    while (!reachedEOF) {
        // Get an empty slot ID
        int slotID;
        if (ctx->pcEmpty->Pop(&slotID) == QUIT) {
            break; // Quit signaled
        }

        // Get pointer to the slot
        char* slotPtr = ctx->bufferManager->GetSlot(slotID);
        int dataSize = ctx->bufferManager->GetDataSize();

        // Read file data into the slot
        DWORD bytesRead;
        BOOL readSuccess = ReadFile(
            hFile,
            slotPtr,
            dataSize,
            &bytesRead,
            NULL
        );

        if (!readSuccess || bytesRead == 0) {
            if (GetLastError() != ERROR_HANDLE_EOF) {
                // Handle error
            }
            reachedEOF = TRUE;
            // No need to process this slot further
            ctx->pcEmpty->Push(&slotID); // Return the slot to the empty pool
            continue;
        }

        // If this isn't the first slot, copy shadow data from previous slot
        if (prevSlotID != -1 && ctx->maxKeywordLength > 0) {
            ctx->bufferManager->CopyShadowBuffer(
                prevSlotID,
                slotID,
                ctx->maxKeywordLength
            );
        }

        // Create MyBuf structure
        MyBuf mb;

        // Special case for first slot (no shadow data)
        if (prevSlotID == -1) {
            mb.ptr = slotPtr;
            mb.size = bytesRead;
        }
        else {
            // For other slots, search starts at shadow buffer
            mb.ptr = ctx->bufferManager->GetShadowBuffer(slotID);
            mb.size = ctx->maxKeywordLength + bytesRead;
        }

        mb.slotID = slotID;

        // Push to full queue
        ctx->pcFull->Push(&mb);

        // Update tracking variables
        prevSlotID = slotID;
    }

    // Signal search threads that no more data is coming
    SetEvent(ctx->eventQuit);

    // Clean up
    CloseHandle(hFile);
    return 0;
}