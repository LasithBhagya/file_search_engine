#include <windows.h>
#include <winioctl.h>
#include <iomanip>
#include <string>
#include <iostream>



// The NTFS_VOLUME_DATA_BUFFER holds the critical disk geometry
// It tells us where the MFT starts (MftStartLcn) and its size (MftValidDataLength)
typedef struct {
    LARGE_INTEGER VolumeSerialNumber;
    LARGE_INTEGER NumberSectors;
    LARGE_INTEGER TotalClusters;
    LARGE_INTEGER FreeClusters;
    LARGE_INTEGER TotalReserved;
    DWORD BytesPerSector;
    DWORD BytesPerCluster;
    DWORD BytesPerFileRecordSegment;
    DWORD ClustersPerFileRecordSegment;
    LARGE_INTEGER MftValidDataLength;
    LARGE_INTEGER MftStartLcn;
    LARGE_INTEGER Mft2StartLcn;
    LARGE_INTEGER MftZoneStart;
    LARGE_INTEGER MftZoneEnd;
} NTFS_VOLUME_INFO;


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
// 15 corresponds to bright white (default)
// 0 = Black       8 = Gray
// 1 = Blue        9 = Light Blue
// 2 = Green       10 = Light Green
// 3 = Aqua        11 = Light Aqua
// 4 = Red         12 = Light Red
// 5 = Purple      13 = Light Purple
// 6 = Yellow      14 = Light Yellow
// 7 = White       15 = Bright White
void SetColor(int color_code) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color_code);
}


int main(int argc, char* argv[]) {
    if (!IsUserAdmin()) {
        SetColor(12);   // Light Red
        std::cerr << "Error: This application requires Administrative privileges to access the MFT (Master File Table).\n";
        SetColor(15);   // White
        std::cout << "Please restart the application as Administrator.\n\n" << "Press Enter to quit.";
        std::cin.get();
        return 1;
    }

    // Open a handle to the raw volume (C: drive)
    // The "\\\\.\\C:" syntax is required for direct disk access.
    HANDLE hVolume = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hVolume == INVALID_HANDLE_VALUE) {
        SetColor(12);   // Light Red
        std::cerr << "Failed to open volume handle. Error: \n";
        SetColor(15);   // White
        std::cout << GetLastError() << std::endl;
        return 1;
    }

    // Query volume data
    NTFS_VOLUME_INFO ntfsData;
    DWORD bytesReturned;
    
    bool success = DeviceIoControl(hVolume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &ntfsData, sizeof(ntfsData), &bytesReturned, NULL);

    if (!success) {
        SetColor(12);
        std::cerr << "Failed to get NTFS volume data. Error: " << GetLastError() << "\n";
        SetColor(15);
        CloseHandle(hVolume);
        return 1;
    }


    // Display the map of our search space
    std::cout << "--- NTFS Volume Geometry ---\n";
    std::cout << "Bytes Per Cluster:      " << ntfsData.BytesPerCluster << "\n";
    std::cout << "Bytes Per File Record:  " << ntfsData.BytesPerFileRecordSegment << "\n";
    std::cout << "MFT Start LCN (Cluster):" << ntfsData.MftStartLcn.QuadPart << "\n";
    std::cout << "MFT Total Size (Bytes): " << ntfsData.MftValidDataLength.QuadPart << "\n";
    
    // Calculate total records (The 'database' size)
    long long totalRecords = ntfsData.MftValidDataLength.QuadPart / ntfsData.BytesPerFileRecordSegment;
    std::cout << "Approx Total Records:   " << totalRecords << "\n";


    // std::cout << "-------------------------------------------------------\n"
    //           << "Successfully connected to C: drive at the sector level.\n"
    //           << "-------------------------------------------------------\n";
    

    // Wait key (Enter)
    std::cout << "\nPress Enter to close.";
    std::cin.get();

    // Clean Up
    CloseHandle(hVolume);

    return 0;
}