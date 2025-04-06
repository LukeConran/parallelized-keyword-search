#pragma once

#include "pch.h"
#include "BufferManager.h"
#include "MyBuf.h"
#include <assert.h>
#include <algorithm> // for min/max

class BufferManager;  // Forward declaration if needed

// Function declarations (prototypes)
void DebugBufferLayout(BufferManager* buffer, int size);
void HexDump(const char* buffer, int length, const char* label);
int RunShadowBufferTests();
void TestFileReading(BufferManager* buffer, const char* filename, int size);
void TestShadowBufferLogic(BufferManager* buffer, int size);