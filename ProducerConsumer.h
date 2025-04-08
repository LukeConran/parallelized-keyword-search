#pragma once

#include "pch.h"

// Define possible return values for Pop
enum PCStatus {
    SUCCESS = 0,
    QUIT = 1
};

class ProducerConsumer {
private:
    HANDLE semaFullSlots;      // Counts filled slots in the queue
    HANDLE semaEmptySlots;     // Counts empty slots in the queue
    HANDLE mutex;              // Protects access to the shared queue
    HANDLE eventQuit;          // Event to signal when threads should quit

    char* buffer;              // Raw buffer to store items
    int head;                  // Index where next item will be removed
    int tail;                  // Index where next item will be added
    int capacity;              // Maximum number of items in the queue
    int itemSize;              // Size of each item in bytes
    int count;                 // Current number of items in the queue

public:
    ProducerConsumer(HANDLE quitEvent, int maxItems, int sizeOfItem) {
        eventQuit = quitEvent;

        mutex = CreateMutex(NULL, FALSE, NULL);
        semaFullSlots = CreateSemaphore(NULL, 0, maxItems, NULL);
        semaEmptySlots = CreateSemaphore(NULL, maxItems, maxItems, NULL);

        capacity = maxItems;
        itemSize = sizeOfItem;
        head = 0;
        tail = 0;
        count = 0;

        buffer = (char*)VirtualAlloc(NULL, capacity * itemSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    ~ProducerConsumer() {
        CloseHandle(mutex);
        CloseHandle(semaFullSlots);
        CloseHandle(semaEmptySlots);

        if (buffer) {
            VirtualFree(buffer, 0, MEM_RELEASE);
        }
    }

    void Push(const void* item) {
        WaitForSingleObject(semaEmptySlots, INFINITE);
        WaitForSingleObject(mutex, INFINITE);

        char* dest = buffer + (tail * itemSize);
        memcpy(dest, item, itemSize);

        tail = (tail + 1) % capacity;
        count++;

        ReleaseMutex(mutex);
        ReleaseSemaphore(semaFullSlots, 1, NULL);
    }

    PCStatus Pop(void* item) {
        HANDLE waitHandles[2] = { semaFullSlots, eventQuit };
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0 + 1) {
            return QUIT;
        }

        WaitForSingleObject(mutex, INFINITE);

        char* src = buffer + (head * itemSize);
        memcpy(item, src, itemSize);

        head = (head + 1) % capacity;
        count--;

        ReleaseMutex(mutex);
        ReleaseSemaphore(semaEmptySlots, 1, NULL);

        return SUCCESS;
    }
};