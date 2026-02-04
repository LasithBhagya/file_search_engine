#include <windows.h>
#include <winioctl.h>
#include <iomanip>
#include <string>
#include <iostream>


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


int main(int argc, char* argv[]) {
    if (!IsUserAdmin()) {
        std::cerr << "Error: This application requires Administrative privileges to access the MFT.\n";
        std::cout << "Please restart the console as Administrator.\n";
        return 1;
    }

    // Open a handle to the raw volume (C: drive)
    // The "\\\\.\\C:" syntax is required for direct disk access.
    HANDLE hVolume = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hVolume == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open volume handle. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::cout << "-------------------------------------------------------\n"
              << "Successfully connected to C: drive at the sector level.\n"
              << "-------------------------------------------------------\n";
    std::cout << "Press enter key to close.";
    std::cin.get();

    // Clean Up
    CloseHandle(hVolume);

    return 0;
}