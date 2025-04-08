#include "pch.h"
#include "ProducerConsumer.h"
#include "FindMaxKeywordLength.h"
#include "KeywordLoader.h"
#include "DiskThread.h"
#include "SearchThread.h"
#include "BufferManager.h"
#include "MyBuf.h"
#include "StatisticsThread.h"



int main(int argc, char* argv[]) {
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

    HANDLE eventQuit = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE statsLock = CreateMutex(NULL, FALSE, NULL);

    ProducerConsumer pcEmpty(eventQuit, numSlots, sizeof(int));
    ProducerConsumer pcFull(eventQuit, numSlots, sizeof(MyBuf));
    BufferManager bufferManager(powerOfTwo, numSlots, nonBufferedIO, maxKeywordLength);

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

    UINT64 totalMatchesFound = 0;
    UINT64 bytesProcessed = 0;
    int activeThreads = 0;

    // Get file size for progress reporting
    HANDLE hFile = CreateFileA(
        wikipediaFilename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    UINT64 fileSize = 0;
    if (hFile != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSizeLarge;
        if (GetFileSizeEx(hFile, &fileSizeLarge)) {
            fileSize = fileSizeLarge.QuadPart;
        }
        CloseHandle(hFile);
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    // int numCores = 5;
    int numCores = sysInfo.dwNumberOfProcessors;

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
        &totalMatchesFound,
        maxKeywordLength,
        &bytesProcessed,
        &activeThreads
    };

    StatisticsThreadContext statsCtx = {
        eventQuit,
        statsLock,
        &totalMatchesFound,
        fileSize,
        &bytesProcessed,
        &activeThreads,
        numCores,
        NULL  // Report file will be opened in the stats thread
    };

    HANDLE hStatsThread = CreateThread(NULL, 0, StatisticsThread, &statsCtx, 0, NULL);
    SetThreadPriority(hStatsThread, ABOVE_NORMAL_PRIORITY_CLASS);

    // Start the disk thread
    HANDLE hDiskThread = CreateThread(NULL, 0, DiskReadThread, &diskCtx, 0, NULL);

    int threadCount = 0;
    HANDLE* threads = new HANDLE[numCores];
    for(int i = 0; i < numCores; i++) {
        WaitForSingleObject(statsLock, INFINITE);
        activeThreads++;
        ReleaseMutex(statsLock);

        HANDLE hSearchThread = CreateThread(NULL, 0, SearchThread, &searchCtx, 0, NULL);
        if (hSearchThread == NULL) {
            printf("Failed to create search thread\n");
            CloseHandle(hDiskThread);
            CloseHandle(eventQuit);
            CloseHandle(statsLock);
            free(keywordStrings);
            free(keywordMatches);
            FreeKeywords(keywords, keywordCount);
            return 1;
        } else {
            threads[threadCount++] = hSearchThread;
            SetThreadPriority(threads[i], IDLE_PRIORITY_CLASS);
            SetThreadAffinityMask(threads[i], 1ULL << i);
        }
    }


    printf("Started threads, waiting for completion...\n");

    WaitForSingleObject(hDiskThread, INFINITE);
    printf("Disk thread finished\n");

    WaitForSingleObject(hStatsThread, INFINITE);
    CloseHandle(hStatsThread);

    if (threadCount > 0) {
        WaitForMultipleObjects(threadCount, threads, TRUE, INFINITE);
        for (int i = 0; i < threadCount; i++) {
            CloseHandle(threads[i]);
        }
    }
    printf("Search thread finished\n");

    printf("Total matches found: %llu\n", totalMatchesFound);

    printf("Keyword matches:\n");
    int limitDisplay = keywordCount;
    for (int i = 0; i < limitDisplay; i++) {
        printf("[%d] %s = %d\n", i, keywords[i].keyword, keywordMatches[i]);
    }

    CloseHandle(hDiskThread);
    CloseHandle(eventQuit);
    CloseHandle(statsLock);
    free(keywordStrings);
    free(keywordMatches);
    FreeKeywords(keywords, keywordCount);

    return 0;
}