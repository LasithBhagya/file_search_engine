#include <iostream>
#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#include <conio.h>

#pragma pack(push, 1)
struct MFT_RECORD_HEADER {
    DWORD magic;           // "FILE"
    WORD  updateSeqOffset;
    WORD  updateSeqSize;
    unsigned __int64 lsn;
    WORD  sequenceNumber;
    WORD  hardLinkCount;
    WORD  attributeOffset;
    WORD  flags;           // 0x01: InUse, 0x02: Directory
    DWORD usedSize;
    DWORD allocatedSize;
    unsigned __int64 baseRecord;
};

struct ATTR_HEADER {
    DWORD typeId;
    DWORD length;
    BYTE  nonResident;
    BYTE  nameLength;
    WORD  nameOffset;
    WORD  flags;
    WORD  attributeId;
};

struct FILENAME_ATTR {
    unsigned __int64 parentDirectory;
    unsigned __int64 creationTime;
    unsigned __int64 modificationTime;
    unsigned __int64 mftModificationTime;
    unsigned __int64 accessTime;
    unsigned __int64 allocatedSize;
    unsigned __int64 realSize;
    DWORD flags;
    DWORD reparseTag;
    BYTE  nameLength;
    BYTE  namespaceType;
    wchar_t name[1];
};
#pragma pack(pop)

// Fixes the "Update Sequence Array" (USA)
// NTFS replaces the last 2 bytes of every sector with a signature.
// This function restores the original bytes so the data is readable.
void ApplyUSAFix(BYTE* record, DWORD recordSize) {
    auto* header = reinterpret_cast<MFT_RECORD_HEADER*>(record);
    WORD* usa = reinterpret_cast<WORD*>(record + header->updateSeqOffset);
    WORD signature = usa[0];

    for (int i = 1; i < header->updateSeqSize; i++) {
        WORD* sectorEnd = reinterpret_cast<WORD*>(record + (i * 512) - 2);
        *sectorEnd = usa[i];
    }
}