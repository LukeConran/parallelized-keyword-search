#pragma once

#include "pch.h"
#include "BufferManager.h"
#include <time.h>
#include <psapi.h>

struct StatisticsThreadContext {
    HANDLE eventQuit;               // Event to signal threads to quit
    HANDLE statsLock;               // Mutex for updating stats
    UINT64* totalMatchesFound;      // Pointer to global match counter
    UINT64 fileSize;                // Total file size in bytes
    UINT64* bytesProcessed;         // Bytes processed so far
    int* activeThreads;             // Number of active search threads
    int numThreads;                 // Total number of search threads
    FILE* reportFile;               // File handle to write reports
};

DWORD WINAPI StatisticsThread(LPVOID param) {
    StatisticsThreadContext* ctx = (StatisticsThreadContext*)param;
    SetThreadPriority(GetCurrentThread(), ABOVE_NORMAL_PRIORITY_CLASS);
    
    UINT64 lastBytesProcessed = 0;
    UINT64 lastMatchesFound = 0;
    
    clock_t startTime = clock();
    clock_t lastPrintTime = startTime;
    
    // Open report file
    FILE* reportFile = nullptr;
    errno_t err = fopen_s(&reportFile, "report.txt", "w");
    if (err != 0 || reportFile == nullptr) {
        fprintf(stderr, "Failed to open report file: report.txt\n");
        reportFile = nullptr;  // Ensure it's null if opening failed
    }
    ctx->reportFile = reportFile;  // Store the file handle in the context
    
    while (WaitForSingleObject(ctx->eventQuit, 2000) == WAIT_TIMEOUT) {
        // Calculate time elapsed
        clock_t currentTime = clock();
        double elapsedSeconds = (double)(currentTime - startTime) / CLOCKS_PER_SEC;
        double intervalSeconds = (double)(currentTime - lastPrintTime) / CLOCKS_PER_SEC;
        
        // Lock stats
        WaitForSingleObject(ctx->statsLock, INFINITE);
        
        // Get current values
        UINT64 currentBytesProcessed = *ctx->bytesProcessed;
        UINT64 currentMatchesFound = *ctx->totalMatchesFound;
        int activeThreads = *ctx->activeThreads;
        
        // Calculate progress
        double percentDone = (double)currentBytesProcessed / ctx->fileSize * 100.0;
        if (percentDone > 100.0) percentDone = 100.0;
        
        // Calculate processing rate in MB/s
        UINT64 bytesDelta = currentBytesProcessed - lastBytesProcessed;
        double mbProcessed = (double)bytesDelta / (1024.0 * 1024.0);
        double mbPerSecond = mbProcessed / intervalSeconds;
        
        // Calculate ETA
        int eta = 0;
        if (percentDone > 0 && percentDone < 100) {
            double remainingPercent = 100.0 - percentDone;
            double timePerPercent = elapsedSeconds / percentDone;
            eta = (int)(remainingPercent * timePerPercent);
        }
        
        // Get memory usage
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        DWORD memoryUsageMB = (DWORD)(pmc.WorkingSetSize / (1024 * 1024));
        
        // Get CPU usage
        FILETIME idleTime, kernelTime, userTime;
        int cpuPercentage = 0;
        
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            static FILETIME prevIdleTime = {0}, prevKernelTime = {0}, prevUserTime = {0};
            
            ULARGE_INTEGER idle, kernel, user, prevIdle, prevKernel, prevUser;
            
            idle.LowPart = idleTime.dwLowDateTime;
            idle.HighPart = idleTime.dwHighDateTime;
            kernel.LowPart = kernelTime.dwLowDateTime;
            kernel.HighPart = kernelTime.dwHighDateTime;
            user.LowPart = userTime.dwLowDateTime;
            user.HighPart = userTime.dwHighDateTime;
            
            prevIdle.LowPart = prevIdleTime.dwLowDateTime;
            prevIdle.HighPart = prevIdleTime.dwHighDateTime;
            prevKernel.LowPart = prevKernelTime.dwLowDateTime;
            prevKernel.HighPart = prevKernelTime.dwHighDateTime;
            prevUser.LowPart = prevUserTime.dwLowDateTime;
            prevUser.HighPart = prevUserTime.dwHighDateTime;
            
            ULONGLONG kernelDiff = kernel.QuadPart - prevKernel.QuadPart;
            ULONGLONG userDiff = user.QuadPart - prevUser.QuadPart;
            ULONGLONG idleDiff = idle.QuadPart - prevIdle.QuadPart;
            ULONGLONG totalDiff = kernelDiff + userDiff;
            
            if (totalDiff > 0) {
                cpuPercentage = (int)(100 - ((idleDiff * 100) / totalDiff));
            }
            
            prevIdleTime = idleTime;
            prevKernelTime = kernelTime;
            prevUserTime = userTime;
        }
        
        ReleaseMutex(ctx->statsLock);
        
        // Format and print statistics
        char buffer[512];
        snprintf(buffer, sizeof(buffer), 
            "%.2f%% ETA %d, %.2f MB/s, %d*, found %llu, CPU %d%% RAM %d MB\n", 
            percentDone, eta, mbPerSecond, activeThreads, currentMatchesFound, 
            cpuPercentage, memoryUsageMB);
        
        printf("%s", buffer);
        if (reportFile) {
            fprintf(reportFile, "%s", buffer);
            fflush(reportFile);  // Ensure it's written immediately
        }
        
        // Update last values
        lastBytesProcessed = currentBytesProcessed;
        lastMatchesFound = currentMatchesFound;
        lastPrintTime = currentTime;
    }
    
    // Print final statistics upon completion
    clock_t endTime = clock();
    double totalSeconds = (double)(endTime - startTime) / CLOCKS_PER_SEC;
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "Total delay %.2f sec, total found %llu\n", 
        totalSeconds, *ctx->totalMatchesFound);
    
    printf("%s", buffer);
    if (reportFile) {
        fprintf(reportFile, "%s", buffer);
        fclose(reportFile);
    }
    
    return 0;
}