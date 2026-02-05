#include "ntfs_engine.hpp"
#include <algorithm>


// Cache the handle so we don't call GetStdHandle repeatedly
static const HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);


enum class MessageType { NORM, ERR, WARN, CUST };       // Normal, Error, Warning, Custom


// Function to check if the app is running as Administrator
bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}


// Changes terminal text color
// 0 = Black       8 = Gray
// 1 = Blue        9 = Light Blue
// 2 = Green       10 = Light Green
// 3 = Aqua        11 = Light Aqua
// 4 = Red         12 = Light Red
// 5 = Purple      13 = Light Purple
// 6 = Yellow      14 = Light Yellow
// 7 = White       15 = Bright White
void SetColor(int color_code) {
    SetConsoleTextAttribute(hConsole, color_code);
}


void PrintMessage(MessageType type, const std::string& message, int color_code = -1) {
    int final_color = color_code;       // final_color variable helps prevent same color change repeating in a row.

    // Default output stream is std::cout. Having out_stream pointer allows easy redirection to std::cerr.
    std::ostream* out_stream = &std::cout;

    // Resolve color and stream based on type
    if (final_color < 0) {
        switch (type) {
            case MessageType::NORM: final_color = 15; break;
            case MessageType::ERR:  final_color = 12; out_stream = &std::cerr; break;
            case MessageType::WARN: final_color = 14; break;
            case MessageType::CUST:
                SetColor(8);
                std::cout << "    --------- The message has not been displayed. Please specify a custom color. ---------\n";
                SetColor(15);
                return;
        }
    }

    SetColor(final_color);
    *out_stream << message << std::endl;        // Puts the message into the output stream
    
    if (final_color != 15) SetColor(15);
}


// -------------------------------------------------------------------------------------------------------------------------------------------------------


int main(int argc, char* argv[]) {
    if (!IsUserAdmin()) {
        PrintMessage(MessageType::ERR, "Error This application requires Administrative privileges to access the MFT (Master File Table).");
        PrintMessage(MessageType::NORM, "Please restart the application as Administrator. \n\n Press Enter to quit.");
        std::cin.get();
        return 1;
    }

    // Open a handle to the raw volume (C: drive)
    // The "\\\\.\\C:" syntax is required for direct disk access to C: volume.
    HANDLE hVolume = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hVolume == INVALID_HANDLE_VALUE) {
        PrintMessage(MessageType::ERR, "Failed to open volume handle. Error:\n");
        PrintMessage(MessageType::NORM, std::to_string(GetLastError()));
        std::cin.get();
        return 1;
    }

    // Get volume data
    NTFS_VOLUME_DATA_BUFFER ntfsData;
    DWORD bytesReturned;

    bool success = DeviceIoControl(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &ntfsData, sizeof(ntfsData), &bytesReturned, NULL);
    if (!success) {
        PrintMessage(MessageType::ERR, "Failed to get NTFS volume data. Error:");
        PrintMessage(MessageType::NORM, std::to_string(GetLastError()));
        CloseHandle(hVolume);
        std::cin.get();
        return 1;
    }


    // Prepare to read the first 100 records (1024 bytes each)
    const DWORD recordSize = ntfsData.BytesPerFileRecordSegment;
    const DWORD recordsToRead = 5000; // Chunk size (tune for speed)
    const DWORD bufferSize = recordSize * recordsToRead;
    std::vector<BYTE> buffer(bufferSize);

    // Calculate Byte offset of MFT
    LARGE_INTEGER mftByteOffset;
    mftByteOffset.QuadPart = ntfsData.MftStartLcn.QuadPart * ntfsData.BytesPerCluster;

    // Set file pointer to start of MFT
    SetFilePointerEx(hVolume, mftByteOffset, NULL, FILE_BEGIN);

    std::wcout << L"Scanning MFT... Standby." << std::endl;

    // Scan Loop
    for (int chunk = 0; chunk < 20; chunk++) { // Just scanning first 100k records for demo
        if (!ReadFile(hVolume, buffer.data(), bufferSize, &bytesReturned, NULL)) break;

        for (DWORD i = 0; i < (bytesReturned / recordSize); i++) {
            BYTE* recordPtr = buffer.data() + (i * recordSize);
            auto* header = reinterpret_cast<MFT_RECORD_HEADER*>(recordPtr);

            if (header->magic != 0x454C4946) continue; // Not a "FILE"
            if (!(header->flags & 0x0001)) continue;   // Not in use

            ApplyUSAFix(recordPtr, recordSize);

            // Walk attributes
            DWORD attrOffset = header->attributeOffset;
            while (attrOffset < header->usedSize) {
                auto* attr = reinterpret_cast<ATTR_HEADER*>(recordPtr + attrOffset);
                if (attr->typeId == 0xFFFFFFFF) break;

                if (attr->typeId == 0x30) { // $FILE_NAME
                    auto* fn = reinterpret_cast<FILENAME_ATTR*>(recordPtr + attrOffset + 24);
                    std::wstring name(fn->name, fn->nameLength);
                    
                    // Filter out system and hidden files
                    if (name[0] != L'$') {
                        std::wcout << L"Found: " << name << std::endl;
                    }
                }
                if (attr->length == 0) break;
                attrOffset += attr->length;
            }
        }
    }


    // Clean Up
    CloseHandle(hVolume);

    std::wcout << L"\nScan complete.\n";

    // Wait key (Enter)
    std::cout << "\nPress Enter to close.";
    std::cin.get();

    return 0;
}