<div align="center">

# SteamFuse

[![Download](.github/assets/download.svg)](https://nightly.link/Platerman/SteamFuse/workflows/build/master/SteamFuse.zip)

</div>

DLL injection helper for Team Fortress 2 on Linux (Proton).  
Wait for `tf_win64.exe` to launch, then inject a DLL via Proton.

## Features

- **Inject** – immediately inject the configured DLL into a running TF2 process.
- **Start and Inject** – launch TF2 via Steam, monitor for `tf_win64.exe`, then inject automatically.
- **Kill** – forcefully terminate the TF2 process (using `pkill -9`).
- **Setup Batch** – configure the injection parameters:
  - Browse for the injector `.exe` and the DLL file.
  - **Auto‑detect** the correct Windows drive letter from the selected files (e.g., `C:`, `D:`, `Z:`).
  - Manually choose a drive letter (C:, D:, or Z:) if auto‑detection fails.
  - Shows converted Windows paths (e.g., `C:\inj.exe`) in real time.
  - Warns if a file is outside the Wine prefix – you can still proceed, but injection may fail.
- **Live log area** – displays the output of `protontricks-launch` for debugging, with a **Clear** button to tidy up.
- **Safety checks** – before injecting, the program verifies that the batch file points to existing files inside the prefix and warns if something is missing.

All settings are saved in the Proton prefix as `Injection.bat` (`~/.local/share/Steam/steamapps/compatdata/440/pfx/drive_c/Injection.bat`).

## Requirements

- Linux with `/proc` filesystem.
- Steam with Proton installed (tested with TF2, AppID 440).
- `protontricks` (provides `protontricks-launch`).
- **Qt6 runtime** – `qt6-qtbase` (usually installed on KDE Plasma; otherwise install from your distro).

### Install protontricks

```bash
sudo dnf install protontricks    # Fedora
sudo apt install protontricks    # Debian/Ubuntu
```

## Building (from source)

1. Install Qt6 development libraries:

   **Fedora:**
   ```bash
   sudo dnf install qt6-qtbase-devel gcc-c++
   ```

   **Ubuntu/Debian:**
   ```bash
   sudo apt install qt6-base-dev build-essential
   ```

2. Clone the repository and enter the source directory:

   ```bash
   git clone https://github.com/yourusername/SteamFuse.git
   cd SteamFuse
   ```

3. Compile:

   ```bash
   qmake6 steamfuse.pro
   make -j$(nproc)
   ```

4. Run:

   ```bash
   ./steamfuse
   ```

## Usage

1. Launch **SteamFuse**.
2. **First‑time setup** – click **Setup Batch**:
   - Browse for the injector `.exe` (e.g., `DLL_Injector-x64-Release.exe`).
   - Browse for the DLL (e.g., `Amalgamx64ReleaseAVX2.dll`).
   - The program will attempt to auto‑detect the drive letter and show the Windows paths.
   - If auto‑detection fails, select the appropriate drive from the dropdown.
   - Click **Create** – the `Injection.bat` file is written to the Proton prefix.
3. **Inject** – if TF2 is already running, click **Inject** to run the batch immediately.
4. **Start and Inject** – launches TF2 via Steam, waits for `tf_win64.exe` to appear, then injects automatically.
5. **Kill** – terminates the TF2 process instantly.
6. **Log area** – check output to verify success; use **Clear** to reset the view.

### Troubleshooting

- If injection fails, check the log for error messages.
- Ensure the injector and DLL are placed inside the Wine prefix (typically `drive_c/`). The setup dialog warns you if you select files outside the prefix.
- If the game launches but injection doesn't happen, increase the delay in `checkProcess()` (in `MainWindow.cpp`) to give the game more startup time.

## License

[DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE](LICENSE) – WTFPL 2.0
