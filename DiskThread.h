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
    SetThreadPriority(GetCurrentThread(), ABOVE_NORMAL_PRIORITY_CLASS);
    DiskThreadContext* ctx = (DiskThreadContext*)param;

    HANDLE hFile = CreateFileA(
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
        SetEvent(ctx->eventQuit); 
        return 1;
    }

    int prevSlotID = -1;
    BOOL reachedEOF = FALSE;

    while (!reachedEOF) {
        int slotID;
        if (ctx->pcEmpty->Pop(&slotID) == QUIT) {
            break;
        }

        char* slotPtr = ctx->bufferManager->GetSlot(slotID);
        int dataSize = ctx->bufferManager->GetDataSize();

        DWORD bytesRead;
        BOOL readSuccess = ReadFile(
            hFile,
            slotPtr,
            dataSize,
            &bytesRead,
            NULL
        );

        if (!readSuccess || bytesRead == 0) {
            if (bytesRead == 0) {
                ctx->pcEmpty->Push(&slotID);
            }
            if (GetLastError() != ERROR_HANDLE_EOF) {
                // Handle error
            }
            reachedEOF = TRUE;

            continue;
        }

        if (prevSlotID != -1 && ctx->maxKeywordLength > 0) {
            ctx->bufferManager->CopyShadowBuffer(
                prevSlotID,
                slotID,
                ctx->maxKeywordLength
            );
        }

        MyBuf mb;
        if (prevSlotID == -1) {
            mb.ptr = slotPtr;
            mb.size = bytesRead;
        } else if(slotID == ctx->bufferManager->GetNumSlots() - 1) {
            mb.ptr = ctx->bufferManager->GetSlot(slotID) - ctx->maxKeywordLength;
            mb.size = ctx->maxKeywordLength * 2 + bytesRead;
        } else {
            mb.ptr = ctx->bufferManager->GetSlot(slotID) - ctx->maxKeywordLength;
            mb.size = ctx->maxKeywordLength + bytesRead;
        }

        mb.slotID = slotID;
        ctx->pcFull->Push(&mb);
        prevSlotID = slotID;
    }

    SetEvent(ctx->eventQuit);
    CloseHandle(hFile);
    return 0;
}