#pragma once

#include "pch.h"

struct MyBuf {
    char* ptr;     // Pointer to start of the search area (may be the main buffer or shadow buffer)
    int size;      // Size of the area to search
    int slotID;    // ID of the slot to return to the pool after searching
};