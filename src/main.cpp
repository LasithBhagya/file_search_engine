#include <windows.h>
#include <winioctl.h>
#include <vector>
#include <string>
#include <iostream>


// Basic MFT Record Header Structure
struct MFT_RECORD_HEADER {
    DWORD magic;           // Should be "FILE" (0x454C4946)
    WORD  updateSeqOffset;
    WORD  updateSeqSize;
    unsigned __int64 lsn;  // Log File Sequence Number
    WORD  sequenceNumber;
    WORD  hardLinkCount;
    WORD  attributeOffset;
    WORD  flags;           // 0x01 = In Use, 0x02 = Directory
    DWORD usedSize;
    DWORD allocatedSize;
    unsigned __int64 baseRecord;
};


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

    // Query volume data
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


    // Calculate Byte offset of MFT
    LARGE_INTEGER mftByteOffset;
    mftByteOffset.QuadPart = ntfsData.MftStartLcn.QuadPart * ntfsData.BytesPerCluster;

    // Prepare to read the first 100 records (1024 bytes each)
    DWORD recordSize = ntfsData.BytesPerFileRecordSegment;
    DWORD bufferSize = recordSize * 100;
    std::vector<BYTE> buffer(bufferSize);

    // Set file pointer to start of MFT
    SetFilePointerEx(hVolume, mftByteOffset, NULL, FILE_BEGIN);

    // Read the chunk
    if (ReadFile(hVolume, buffer.data(), bufferSize, &bytesReturned, NULL)) {
        PrintMessage(MessageType::CUST, "Successfully read" + std::to_string(bytesReturned / recordSize) + " MFT records.\n", 10);

        for (int i = 0; i < 100; i++) {
            MFT_RECORD_HEADER* header = reinterpret_cast<MFT_RECORD_HEADER*>(buffer.data() + (i * recordSize));
            
            // Check if this record is valid/active
            if (header->magic == 0x454C4946) { // "FILE" in hex
                bool isDirectory = header->flags & 0x0002;
                std::cout << "Record [" << i << "]: " 
                          << (isDirectory ? "[DIR]  " : "[FILE] ")
                          << "Used Size: " << header->usedSize << " bytes\n";
            }
        }
    } else {
        PrintMessage(MessageType::ERR, "Read failed. Error: ");
        PrintMessage(MessageType::NORM, std::to_string(GetLastError()));
    }


    // Wait key (Enter)
    std::cout << "\nPress Enter to close.";
    std::cin.get();

    // Clean Up
    CloseHandle(hVolume);

    return 0;
}