#pragma once

#include "pch.h"
#include "ProducerConsumer.h"
#include "BufferManager.h"
#include "MyBuf.h"

struct SearchThreadContext {
    ProducerConsumer* pcEmpty;      // Queue for empty slots
    ProducerConsumer* pcFull;       // Queue for filled slots
    const char** keywords;          // Array of keywords to search for
    int keywordCount;               // Number of keywords in the array
    int* keywordMatches;            // Array to count matches for each keyword
    HANDLE eventQuit;               // Event to signal threads to quit
    HANDLE statsLock;               // Mutex for updating stats
    UINT64* totalMatchesFound;      // Pointer to global match counter
    int maxKeywordLength;           // Maximum length of keywords
    UINT64* bytesProcessed;         // Pointer to bytes processed counter
    int* activeThreads;             // Pointer to active threads counter
};

DWORD WINAPI SearchThread(LPVOID param) {
    SearchThreadContext* ctx = (SearchThreadContext*)param;

    while (TRUE) {
        MyBuf mb;
        if (ctx->pcFull->Pop(&mb) == QUIT) {
            break;
        }
        
        mb.ptr[mb.size] = '\0';

        WaitForSingleObject(ctx->statsLock, INFINITE);
        *ctx->bytesProcessed += mb.size;
        ReleaseMutex(ctx->statsLock);

        UINT64 localMatches = 0;
        for (int i = 0; i < ctx->keywordCount; i++) {
            const char* pos = mb.ptr;
            int keywordMatches = 0;

            while ((pos = strstr(pos, ctx->keywords[i])) != NULL) {
                if(pos - mb.ptr < mb.size) {
                    localMatches++;
                    keywordMatches++;
                    pos += 1;
                }
                else {
                    break;
                }
            }

            if (keywordMatches > 0) {
                WaitForSingleObject(ctx->statsLock, INFINITE);
                ctx->keywordMatches[i] += keywordMatches;
                ReleaseMutex(ctx->statsLock);
            }
        }

        if (localMatches > 0) {
            WaitForSingleObject(ctx->statsLock, INFINITE);
            *ctx->totalMatchesFound += localMatches;
            ReleaseMutex(ctx->statsLock);
        }

        ctx->pcEmpty->Push(&mb.slotID);
    }

    WaitForSingleObject(ctx->statsLock, INFINITE);
    (*ctx->activeThreads)--;
    ReleaseMutex(ctx->statsLock);

    return 0;
}