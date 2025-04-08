#pragma once

#include "pch.h"

int FindMaxKeywordLength(const char* keywordFilename) {
    HANDLE hFile = CreateFileA(keywordFilename, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    int maxLength = 0;
    char buffer[1024];
    DWORD bytesRead;
    int currentLineLength = 0;

    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; i++) {
            if (buffer[i] == '\n' || buffer[i] == '\r') {
                if (currentLineLength > maxLength) {
                    maxLength = currentLineLength;
                }
                currentLineLength = 0;
            }
            else {
                currentLineLength++;
            }
        }
    }

    if (currentLineLength > maxLength) {
        maxLength = currentLineLength;
    }

    CloseHandle(hFile);
    return maxLength;
}