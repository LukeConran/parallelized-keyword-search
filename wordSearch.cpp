#include "pch.h"
#include "ProducerConsumer.h"
#include "FindMaxKeywordLength.h"
#include "KeywordLoader.h"
#include "DiskThread.h"
#include "SearchThread.h"
#include "Tests.h"
#include "BufferManager.h"
#include "MyBuf.h"



int main(int argc, char* argv[]) {

    //RunShadowBufferTests();

    // Parse command-line arguments
    if (argc != 7) {
        printf("Usage: %s <keywords.txt> <wikipedia.txt> <powerOfTwo> <numSlots> <nonBufferedIO> <strstr|RK>\n", argv[0]);
        return 1;
    }

    const char* keywordFilename = argv[1];
    const char* wikipediaFilename = argv[2];
    int powerOfTwo = atoi(argv[3]);
    int numSlots = atoi(argv[4]);
    BOOL nonBufferedIO = atoi(argv[5]);
    const char* searchAlgorithm = argv[6];

    // Check if Rabin-Karp is requested but not implemented
    if (strcmp(searchAlgorithm, "RK") == 0) {
        printf("RK is not supported\n");
        return 1;
    }

    // Find the maximum keyword length
    int maxKeywordLength = FindMaxKeywordLength(keywordFilename);
    printf("Max keyword length: %d\n", maxKeywordLength);

    // Create the quit event for clean termination
    HANDLE eventQuit = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Initialize the mutex for statistics
    HANDLE statsLock = CreateMutex(NULL, FALSE, NULL);

    // Initialize the producer-consumer queues
    ProducerConsumer pcEmpty(eventQuit, numSlots, sizeof(int));
    ProducerConsumer pcFull(eventQuit, numSlots, sizeof(MyBuf));

    // Initialize the buffer manager
    BufferManager bufferManager(powerOfTwo, numSlots, nonBufferedIO, maxKeywordLength);

    // Load keywords
    KeywordEntry* keywords = nullptr;
    int keywordCount = LoadKeywords(keywordFilename, &keywords);

    if (keywordCount == 0) {
        printf("Failed to load keywords from %s\n", keywordFilename);
        CloseHandle(eventQuit);
        CloseHandle(statsLock);
        return 1;
    }

    printf("Loaded %d keywords\n", keywordCount);

    // Create keyword strings array for search threads
    const char** keywordStrings = (const char**)malloc(keywordCount * sizeof(char*));
    int* keywordMatches = (int*)calloc(keywordCount, sizeof(int));

    for (int i = 0; i < keywordCount; i++) {
        keywordStrings[i] = keywords[i].keyword;
    }

    // Initialize empty slots
    for (int i = 0; i < numSlots; i++) {
        pcEmpty.Push(&i);
    }

    // Track total matches
    UINT64 totalMatchesFound = 0;

    // Create disk thread context
    DiskThreadContext diskCtx = {
        wikipediaFilename,
        &pcEmpty,
        &pcFull,
        &bufferManager,
        eventQuit,
        nonBufferedIO,
        maxKeywordLength
    };

    // Create search thread context
    SearchThreadContext searchCtx = {
        &pcEmpty,
        &pcFull,
        keywordStrings,
        keywordCount,
        keywordMatches,
        eventQuit,
        statsLock,
        &totalMatchesFound
    };

    // Start the disk thread
    HANDLE hDiskThread = CreateThread(NULL, 0, DiskReadThread, &diskCtx, 0, NULL);

    // Start a single search thread for testing
    HANDLE hSearchThread = CreateThread(NULL, 0, SearchThread, &searchCtx, 0, NULL);

    // Simple progress reporting
    printf("Started threads, waiting for completion...\n");

    // Wait for the disk thread to finish
    WaitForSingleObject(hDiskThread, INFINITE);
    printf("Disk thread finished\n");

    // Signal the search thread to finish (should happen automatically)
    WaitForSingleObject(hSearchThread, INFINITE);
    printf("Search thread finished\n");

    // Print results
    printf("Total matches found: %llu\n", totalMatchesFound);

    // Print top 10 keyword matches (or fewer if there are less than 10 keywords)
    printf("Keyword matches:\n");
    int limitDisplay = keywordCount;
    for (int i = 0; i < limitDisplay; i++) {
        printf("[%d] %s = %d\n", i, keywords[i].keyword, keywordMatches[i]);
    }

    // Clean up
    CloseHandle(hDiskThread);
    CloseHandle(hSearchThread);
    CloseHandle(eventQuit);
    CloseHandle(statsLock);

    free(keywordStrings);
    free(keywordMatches);
    FreeKeywords(keywords, keywordCount);

    return 0;
}