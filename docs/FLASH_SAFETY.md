# Flash safety gate and recovery

The current Tab5 runs a working dice roller. **The camera firmware will replace its application; that is intentional**, but only after the installed flash has been permanently preserved.

1. On Windows PowerShell 5.1, connect the Tab5 by USB-C. Confirm correct COM port (historically COM5).
2. Install `python -m pip install esptool==5.6.0` and authenticate `gh auth login`.
3. Create a **private** GitHub repository for recovery images if none exists. Keep it separate from the public `Tab5LASR` repository.
4. Run: `powershell -ExecutionPolicy Bypass -File .\tools\BACKUP_TAB5_DICE.ps1 -Port COM5 -PrivateRepo cbw29512/YOUR_PRIVATE_BACKUP_REPO`
5. Confirm the script reports **PERMANENT PRIVATE RELEASE VERIFIED**. Record its SHA-256 in the project issue for auditing, but **never** paste secrets or flash contents.
6. Before flashing new firmware, verify its image chip revision compatibility with your ESP32-P4 v1.3, exact application offset/partition layout and a size within the 1MiB existing application partition.
7. **Application-only update** at `0x10000` only after passing all gates. Never erase the 16MiB device or flash a mismatched bootloader/partition table.
8. Recheck that the Tab5 boots, camera enumerates, and power/charging behavior remains safe.

Restore is intentionally a separate, explicit operation. **Do not attempt a full-device restore based on this checklist alone.** Confirm actual hardware flash mode, exact backup SHA-256, and safe recovery procedure first.

This script **does not** itself create a private GitHub repository or run a flash operation.
