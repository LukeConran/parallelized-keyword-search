#pragma once

#include "pch.h"

// Structure to hold keyword information
struct KeywordEntry {
    char* keyword;
    int count;
};

// Function to load keywords from a file
int LoadKeywords(const char* keywordFilename, KeywordEntry** keywordsOut) {
    HANDLE hFile = CreateFileA(keywordFilename, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    // First pass: count the number of lines
    int lineCount = 0;
    char buffer[4096];
    DWORD bytesRead;
    BOOL lastCharWasNewline = TRUE;

    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n') {
                lineCount++;
                lastCharWasNewline = TRUE;
            }
            else if (buffer[i] != '\r') {
                lastCharWasNewline = FALSE;
            }
        }
    }

    // If the file doesn't end with a newline, count the last line
    if (!lastCharWasNewline) {
        lineCount++;
    }

    // Allocate memory for keywords
    *keywordsOut = (KeywordEntry*)malloc(lineCount * sizeof(KeywordEntry));
    if (!*keywordsOut) {
        CloseHandle(hFile);
        return 0;
    }

    // Reset file pointer to beginning of file
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

    // Second pass: read the keywords
    char lineBuffer[1024];
    int linePos = 0;
    int keywordIndex = 0;

    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n' || buffer[i] == '\r') {
                if (linePos > 0) {
                    // Remove trailing spaces
                    while (linePos > 0 && isspace((unsigned char)lineBuffer[linePos - 1])) {
                        linePos--;
                    }

                    // Allocate and copy the keyword
                    (*keywordsOut)[keywordIndex].keyword = (char*)malloc(linePos + 1);
                    if ((*keywordsOut)[keywordIndex].keyword) {
                        memcpy((*keywordsOut)[keywordIndex].keyword, lineBuffer, linePos);
                        (*keywordsOut)[keywordIndex].keyword[linePos] = '\0';
                        (*keywordsOut)[keywordIndex].count = 0;
                        keywordIndex++;
                    }

                    linePos = 0;
                }

                // Skip additional line terminators (CR+LF pair)
                if (buffer[i] == '\r' && i + 1 < bytesRead && buffer[i + 1] == '\n') {
                    i++;
                }
            }
            else {
                // Add character to line buffer
                if (linePos < sizeof(lineBuffer) - 1) {
                    lineBuffer[linePos++] = buffer[i];
                }
            }
        }
    }

    // Process the last line if needed
    if (linePos > 0) {
        // Remove trailing spaces
        while (linePos > 0 && isspace((unsigned char)lineBuffer[linePos - 1])) {
            linePos--;
        }

        // Allocate and copy the keyword
        (*keywordsOut)[keywordIndex].keyword = (char*)malloc(linePos + 1);
        if ((*keywordsOut)[keywordIndex].keyword) {
            memcpy((*keywordsOut)[keywordIndex].keyword, lineBuffer, linePos);
            (*keywordsOut)[keywordIndex].keyword[linePos] = '\0';
            (*keywordsOut)[keywordIndex].count = 0;
            keywordIndex++;
        }
    }

    CloseHandle(hFile);
    return keywordIndex;
}

// Function to free keyword memory
void FreeKeywords(KeywordEntry* keywords, int count) {
    if (!keywords) return;

    for (int i = 0; i < count; i++) {
        if (keywords[i].keyword) {
            free(keywords[i].keyword);
        }
    }

    free(keywords);
}