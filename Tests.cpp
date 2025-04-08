#include "pch.h"
#include "Tests.h"

// Utility function to print a hex dump of buffer
void HexDump(const char* buffer, int size, const char* label) {
    printf("%s (size: %d):\n", label, size);
    printf("Offset | Hex                                | ASCII\n");
    printf("-------|------------------------------------|----------------\n");
    
    for (int offset = 0; offset < size; offset += 16) {
        printf("%06X | ", offset);
        
        // Print hex values
        for (int i = 0; i < 16; i++) {
            if (offset + i < size) {
                printf("%02X ", (unsigned char)buffer[offset + i]);
            } else {
                printf("   ");
            }
        }
        
        printf("| ");
        
        // Print ASCII representation
        for (int i = 0; i < 16; i++) {
            if (offset + i < size) {
                char c = buffer[offset + i];
                printf("%c", (c >= 32 && c <= 126) ? c : '.');
            } else {
                printf(" ");
            }
        }
        
        printf("\n");
    }
    printf("\n");
}

// Test function to verify shadow buffer logic
void TestShadowBufferLogic(BufferManager* bufferManager, int maxKeywordLength) {
    printf("\n======= SHADOW BUFFER LOGIC TEST =======\n");
    
    // Create a test pattern that spans slots
    char testPattern[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int patternLength = strlen(testPattern);
    
    int dataSize = bufferManager->GetDataSize();
    int shadowSize = bufferManager->GetShadowSize();
    
    printf("Data size: %d, Shadow size: %d, Max keyword length: %d\n", 
           dataSize, shadowSize, maxKeywordLength);
    
    // Clear both slots to ensure clean test
    char* slot0 = bufferManager->GetSlot(0);
    char* slot1 = bufferManager->GetSlot(1);
    memset(slot0, '.', dataSize);
    memset(slot1, '.', dataSize);
    
    // Fill the end of slot 0 with the first part of the pattern
    int prefixLength = (std::min)(maxKeywordLength, patternLength);
    memcpy(slot0 + dataSize - prefixLength, testPattern, prefixLength);
    
    printf("Set the last %d bytes of slot 0 to: %.10s...\n", prefixLength, testPattern);
    
    // Copy shadow data from slot 0 to slot 1
    bufferManager->CopyShadowBuffer(0, 1, prefixLength);
    
    // Fill the beginning of slot 1 with the rest of the pattern
    int remainingLength = (std::min)(patternLength - prefixLength, 20);
    memcpy(slot1, testPattern + prefixLength, remainingLength);
    
    printf("Set the first %d bytes of slot 1 to: %.10s...\n", 
           remainingLength, testPattern + prefixLength);
    
    // Verify the shadow buffer of slot 1 contains the expected data
    char* shadowBuf1 = bufferManager->GetShadowBuffer(1);
    
    printf("\nVerifying shadow buffer content:\n");
    HexDump(shadowBuf1, prefixLength, "Shadow buffer of slot 1");
    HexDump(slot0 + dataSize - prefixLength, prefixLength, "End of slot 0 (expected to match)");
    
    bool shadowMatchesExpected = true;
    for (int i = 0; i < prefixLength; i++) {
        if (shadowBuf1[i] != testPattern[i]) {
            printf("Shadow buffer verification failed at position %d: expected '%c', got '%c'\n",
                   i, testPattern[i], shadowBuf1[i]);
            shadowMatchesExpected = false;
        }
    }
    
    if (shadowMatchesExpected) {
        printf("Shadow buffer contains expected data\n");
    }
    
    // Create a test keyword that spans the boundary
    int spanKeywordLength = (std::min)(prefixLength + remainingLength - 2, 15);
    int startPos = prefixLength - spanKeywordLength / 2;
    
    char spanKeyword[20];
    strncpy_s(spanKeyword, testPattern + startPos, spanKeywordLength);
    spanKeyword[spanKeywordLength] = '\0';
    
    printf("\nTesting keyword that spans buffer boundary: '%s'\n", spanKeyword);
    printf("This keyword starts %d bytes before the end of slot 0\n", 
           spanKeywordLength / 2);
    
    // Create a test MyBuf structure like DiskThread would
    MyBuf mb;
    mb.ptr = shadowBuf1;
    mb.size = prefixLength + remainingLength;
    mb.slotID = 1;
    
    // Null-terminate for strstr (make sure there's space for this!)
    char* searchBuffer = (char*)malloc(mb.size + 1);
    memcpy(searchBuffer, mb.ptr, mb.size);
    searchBuffer[mb.size] = '\0';
    
    // Try to find the pattern
    char* found = strstr(searchBuffer, spanKeyword);
    if (found) {
        int offset = found - searchBuffer;
        printf("Successfully found keyword '%s' at offset %d in search buffer\n", 
               spanKeyword, offset);
        
        // Show some context around the match
        printf("Context: \"");
        int contextStart = (std::max)(0, offset - 5);
        int contextEnd = (std::min)(mb.size, offset + spanKeywordLength + 5);
        for (int i = contextStart; i < contextEnd; i++) {
            printf("%c", searchBuffer[i]);
        }
        printf("\"\n");
    } else {
        printf("Failed to find keyword '%s' spanning buffer boundary\n", spanKeyword);
    }
    
    free(searchBuffer);
    
    // Test multiple spans
    printf("\n======= MULTIPLE BOUNDARY SPANS TEST =======\n");
    
    // Clear slots again
    memset(slot0, '.', dataSize);
    memset(slot1, '.', dataSize);
    
    // We'll create a repeating pattern that should result in multiple matches
    const char* repeatingPattern = "ABABABABABABABABABABABABABABAB";
    int repeatLen = strlen(repeatingPattern);
    
    // Fill the end of slot 0
    memcpy(slot0 + dataSize - prefixLength, repeatingPattern, prefixLength);
    
    // Copy to shadow buffer of slot 1
    bufferManager->CopyShadowBuffer(0, 1, prefixLength);
    
    // Fill the start of slot 1
    memcpy(slot1, repeatingPattern + prefixLength % repeatLen, (std::min)(remainingLength, repeatLen));
    
    // Target is "BABABA" which should appear multiple times across the boundary
    const char* target = "BABABA";
    printf("Searching for pattern '%s' in a buffer filled with '%s'\n", 
           target, repeatingPattern);
    
    // Create search buffer
    searchBuffer = (char*)malloc(prefixLength + remainingLength + 1);
    memcpy(searchBuffer, shadowBuf1, prefixLength + remainingLength);
    searchBuffer[prefixLength + remainingLength] = '\0';
    
    // Find all occurrences
    int matchCount = 0;
    char* pos = searchBuffer;
    while ((pos = strstr(pos, target)) != NULL) {
        int offset = pos - searchBuffer;
        matchCount++;
        printf("Match %d found at offset %d\n", matchCount, offset);
        
        // Move past this character to find overlapping matches
        pos++;
    }
    
    printf("Total matches found: %d\n", matchCount);
    printf("Matches specifically across the boundary would be around offset %d\n",
           prefixLength - strlen(target) / 2);
    
    free(searchBuffer);
}

// Visual representation of buffer layout
void DebugBufferLayout(BufferManager* bm, int slotID) {
    printf("\n======= BUFFER LAYOUT FOR SLOT %d =======\n", slotID);
    
    char* shadowBuf = bm->GetShadowBuffer(slotID);
    char* dataBuf = bm->GetSlot(slotID);
    int shadowSize = bm->GetShadowSize();
    int dataSize = bm->GetDataSize();
    int slotSize = bm->GetSlotSize();
    
    printf("Full slot size: %d bytes\n", slotSize);
    printf("Shadow buffer size: %d bytes\n", shadowSize);
    printf("Data buffer size: %d bytes\n", dataSize);
    
    printf("\nMemory layout:\n");
    printf("Slot base address: %p\n", shadowBuf);
    printf("Shadow buffer start: %p\n", shadowBuf);
    printf("Data buffer start: %p (offset %ld from slot base)\n", 
           dataBuf, (char*)dataBuf - (char*)shadowBuf);
    
    // If the buffers have been filled, display content
    if (slotID > 0) {
        printf("\nShadow buffer preview:\n");
        HexDump(shadowBuf, (std::min)(shadowSize, 64), "Shadow buffer");
        
        printf("\nStart of data buffer:\n");
        HexDump(dataBuf, (std::min)(32, dataSize), "Start of data");
        
        printf("\nEnd of data buffer:\n");
        HexDump(dataBuf + dataSize - (std::min)(32, dataSize), 
                (std::min)(32, dataSize), "End of data");
    }
}

// Test to simulate reads spanning buffer boundaries
void TestFileReading(BufferManager* bm, const char* filename, int maxKeywordLength) {
    printf("\n======= FILE READING SIMULATION TEST =======\n");
    
    HANDLE hFile = CreateFile(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Failed to open test file: %s\n", filename);
        return;
    }
    
    int dataSize = bm->GetDataSize();
    int slotSize = bm->GetSlotSize();
    
    // Simulate reading into slot 0
    char* slot0 = bm->GetSlot(0);
    DWORD bytesRead0;
    ReadFile(hFile, slot0, dataSize, &bytesRead0, NULL);
    
    printf("Read %u bytes into slot 0\n", bytesRead0);
    HexDump(slot0 + dataSize - (std::min)((int)bytesRead0, 32), 
            (std::min)((int)bytesRead0, 32), "End of slot 0 data");
    
    // Copy shadow data from slot 0 to slot 1
    bm->CopyShadowBuffer(0, 1, maxKeywordLength);
    
    // Read into slot 1
    char* slot1 = bm->GetSlot(1);
    DWORD bytesRead1;
    ReadFile(hFile, slot1, dataSize, &bytesRead1, NULL);
    
    printf("Read %u bytes into slot 1\n", bytesRead1);
    
    // Verify shadow buffer
    char* shadowBuf1 = bm->GetShadowBuffer(1);
    printf("Shadow buffer content for slot 1:\n");
    HexDump(shadowBuf1, (std::min)(maxKeywordLength, 64), "Shadow buffer");
    
    // Now create a MyBuf structure like in your DiskReadThread
    MyBuf mb;
    mb.ptr = shadowBuf1;
    mb.size = maxKeywordLength + bytesRead1;
    mb.slotID = 1;
    
    // Create a properly null-terminated search buffer
    char* searchBuffer = (char*)malloc(mb.size + 1);
    if (!searchBuffer) {
        printf("Failed to allocate search buffer\n");
        CloseHandle(hFile);
        return;
    }
    
    memcpy(searchBuffer, mb.ptr, mb.size);
    searchBuffer[mb.size] = '\0';
    
    // Print the content around the boundary
    printf("\nContent spanning the boundary (last %d bytes of slot 0 + first %d bytes of slot 1):\n",
           (std::min)(maxKeywordLength, 32), (std::min)(32, (int)bytesRead1));
           
    int boundaryPreview = (std::min)(maxKeywordLength, 32) + (std::min)(32, (int)bytesRead1);
    for (int i = 0; i < boundaryPreview; i++) {
        char c = searchBuffer[i];
        printf("%c", (c >= 32 && c <= 126) ? c : '.');
    }
    printf("\n");
    
    // Test with a known keyword near the boundary
    // We'll scan the buffer and find any words around the boundary
    printf("\nWords near the boundary:\n");
    
    // Simple word extraction (this is just for debugging)
    char word[256];
    int wordLen = 0;
    bool inWord = false;
    
    for (int i = maxKeywordLength - 32; i < maxKeywordLength + 32 && i < mb.size; i++) {
        char c = searchBuffer[i];
        
        if (isalpha(c) || c == '\'') {
            if (!inWord) {
                inWord = true;
                wordLen = 0;
            }
            if (wordLen < 255) {
                word[wordLen++] = c;
            }
        } else {
            if (inWord) {
                word[wordLen] = '\0';
                int relativePos = i - wordLen - maxKeywordLength;
                printf("Found word '%s' at relative position %d from boundary (negative = slot 0, positive = slot 1)\n",
                       word, relativePos);
                inWord = false;
            }
        }
    }
    
    if (inWord) {
        word[wordLen] = '\0';
        printf("Found word '%s' at the end of the search range\n", word);
    }
    
    free(searchBuffer);
    CloseHandle(hFile);
}

// Create a test file with keywords specifically at boundary positions
void CreateBoundaryTestFile(const char* filename) {
    printf("\n======= CREATING BOUNDARY TEST FILE =======\n");

    FILE* f = NULL;  // Initialize to NULL
    errno_t err = fopen_s(&f, filename, "w");  // Pass pointer to FILE*
    if (err != 0 || f == NULL) {  // Check for error or NULL file pointer
        printf("Failed to create test file: %s\n", filename);
        return;
    }

    // Write a repeating pattern with some keywords
    const char* testKeywords[] = { "computer", "systems", "boundary", "testing" };
    const int keywordCount = 4;

    // Create content with keywords at various positions
    for (int i = 0; i < 20; i++) {
        // Add varying padding to shift positions
        for (int j = 0; j < i * 3; j++) {
            fputc('_', f);
        }

        // Add a keyword that might span boundary
        fprintf(f, "%s", testKeywords[i % keywordCount]);

        // Add separator
        fprintf(f, "____");
    }

    // Add some special test cases
    fprintf(f, "\n\nSpecial test cases:\n");

    // 1. Overlapping matches
    fprintf(f, "testest\n");

    // 2. Keywords right next to each other
    fprintf(f, "computersystems\n");

    // 3. Repeated keywords
    fprintf(f, "computercomputer\n");
    fprintf(f, "systemssystems\n");

    // 4. Text with varying case
    fprintf(f, "ComputerSystems\n");

    fclose(f);
    printf("Created test file: %s\n", filename);
}

// Main function to run all tests
int RunShadowBufferTests() {
    printf("======= SHADOW BUFFER TEST SUITE =======\n");
    
    // Create a test file
    const char* testFilename = "shadow_buffer_test.txt";
    CreateBoundaryTestFile(testFilename);
    
    // Set up test parameters 
    int powerOfTwo = 8;       // 256 bytes buffer
    int numSlots = 5;         // 5 slots
    int maxKeywordLength = 16; // Max keyword length
    
    // Initialize BufferManager for testing
    BufferManager* bm = new BufferManager(powerOfTwo, numSlots, FALSE, maxKeywordLength);
    
    // Run tests
    DebugBufferLayout(bm, 0);
    TestShadowBufferLogic(bm, maxKeywordLength);
    TestFileReading(bm, testFilename, maxKeywordLength);
    
    // Clean up
    delete bm;
    
    printf("\n======= TESTS COMPLETED =======\n");
    return 0;
}