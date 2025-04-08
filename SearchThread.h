#pragma once

#include "pch.h"
#include "ProducerConsumer.h"
#include "BufferManager.h"
#include "MyBuf.h"

// Simple structure to hold search thread parameters 
struct SearchThreadContext {
    ProducerConsumer* pcEmpty;      // Queue for empty slots
    ProducerConsumer* pcFull;       // Queue for filled slots
    const char** keywords;          // Array of keywords to search for
    int keywordCount;               // Number of keywords in the array
    int* keywordMatches;            // Array to count matches for each keyword
    HANDLE eventQuit;               // Event to signal threads to quit
    HANDLE statsLock;               // Mutex for updating stats
    UINT64* totalMatchesFound;      // Pointer to global match counter
};

DWORD WINAPI SearchThread(LPVOID param) {
    SearchThreadContext* ctx = (SearchThreadContext*)param;
    SetThreadPriority(GetCurrentThread(), IDLE_PRIORITY_CLASS);
    SetThreadAffinityMask(GetCurrentThread(), 1ULL << (GetCurrentProcessorNumber() % 64)); //need to alter this so it is for any computer

    while (TRUE) {
        MyBuf mb;
        if (ctx->pcFull->Pop(&mb) == QUIT) {
            break;
        }
        char nullTerminator = mb.ptr[mb.size];
        mb.ptr[mb.size] = '\0';

        UINT64 localMatches = 0;
        for (int i = 0; i < ctx->keywordCount; i++) {
            const char* pos = mb.ptr;
            int keywordMatches = 0;

            while ((pos = strstr(pos, ctx->keywords[i])) != NULL) {
                localMatches++;
                keywordMatches++;
                pos += 1;
            }

            if (keywordMatches > 0) {
                WaitForSingleObject(ctx->statsLock, INFINITE);
                ctx->keywordMatches[i] += keywordMatches;
                ReleaseMutex(ctx->statsLock);
            }
        }

        mb.ptr[mb.size] = nullTerminator;

        if (localMatches > 0) {
            WaitForSingleObject(ctx->statsLock, INFINITE);
            *ctx->totalMatchesFound += localMatches;
            ReleaseMutex(ctx->statsLock);
        }

        ctx->pcEmpty->Push(&mb.slotID);
    }

    return 0;
}