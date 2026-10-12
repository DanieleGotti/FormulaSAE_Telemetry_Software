# ERB Telemetry Software – Developer Guide

This guide is for someone who has **never seen this software before**. It explains, step by step:

1. what the software does and how the repository is organised;
2. how to open, build and run it;
3. how to test it **without the car** using the Python simulators;
4. how to **add, change or remove a value in the telemetry packets**, both in the backend and in the UI, and how to update the Python simulators accordingly.

Everything in the C++ code is documented with Doxygen-style comments (`/** @brief ... */`). When this guide says "see `X.hpp`", open that file: the header comment at the top explains the class in detail.

---

## 1. What the software does

The ECU of the car sends telemetry over a serial link (UART, 500000 baud). This desktop application:

- **Live mode** – reads the serial port, decodes the packets, shows the values in real time in many windows and can record them to a CSV file.
- **Playback mode** – opens a recording (`.bin` file written by the ECU to its SD card, or a `.csv` file produced by this software), lets you scroll through time with a slider and exports a `.bin` to a high-resolution `.csv`.

It is written in **C++20** with **Dear ImGui** + **ImPlot** for the interface, on top of **GLFW/OpenGL**.

### The two telemetry packets

| Packet | Start bytes | Period | Content |
|---|---|---|---|
| **A** | `0xCC 0x11` | every 5 ms (200 Hz) | wheel speeds, pedals, steering, inverters, temperatures, state machines, ... |
| **B** | `0xEE 0x11` | every 200 ms | battery (EMMA): state of charge, voltage and temperature of the 14 modules |

Every packet has the same structure (all values little-endian):

```
[start byte 1][start byte 2][timestamp: uint32, 5 ms ticks][fields ...][CRC16: uint16]
```

The CRC is CRC-16/CCITT-FALSE computed over all the bytes before it. Every A packet becomes one row of data. The latest B packet values are copied into every A row.

---

## 2. Tools you need

### Windows (the team's main platform)

| Tool | Why | Where |
|---|---|---|
| **Git** | get the code and its submodules | https://git-scm.com |
| **MSYS2 + MinGW-w64 GCC** | C++ compiler (`g++`) and `mingw32-make` | https://www.msys2.org – then in the *MSYS2 MinGW64* shell: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make` |
| **CMake ≥ 3.15** | build system | https://cmake.org/download |
| **Python 3** (with Tkinter, included in the Windows installer) | simulators and scripts | https://www.python.org |
| **VS Code** (recommended) | editor | https://code.visualstudio.com – extensions: *C/C++*, *CMake Tools* |

Make sure `C:\msys64\mingw64\bin` is in the Windows `PATH`, so that `g++` and `mingw32-make` work from any terminal. Check with:

```bash
g++ --version
```

```bash
cmake --version
```

### Linux

```bash
sudo apt install build-essential cmake git python3 python3-tk libgl1-mesa-dev xorg-dev libwayland-dev libxkbcommon-dev
```

(`xorg-dev`, `libwayland-dev` and `libxkbcommon-dev` are needed to build GLFW.)

### macOS

Install the Xcode command line tools (`xcode-select --install`), then CMake and Python (e.g. with Homebrew: `brew install cmake python`).

### Python packages (all platforms)

```bash
pip install pyserial
```

---

## 3. Getting the code

```bash
git clone <repository-url> telemetry
```

```bash
cd telemetry
```

The libraries GLFW, GLM, ImGui and ImPlot are **git submodules** and must be downloaded once:

```bash
git submodule update --init external/glfw external/glm external/imgui external/implot
```

> Use the explicit list above: `external/SDL` and `external/libusb` are registered as submodules without a URL and are not used by the build, so a plain `git submodule update --init` may stop with an error.

`stb_image.h` is already in the repository (`external/stb_image/`). The JSON library **jsoncons** is downloaded automatically by CMake the first time you configure (this needs an internet connection).

### Opening the project

In VS Code: **File → Open Folder…** and choose the repository root (the folder that contains `CMakeLists.txt`). Do not open `src/` or `include/` alone, otherwise the include paths will not resolve.

---

## 4. Repository tour

```
telemetry/
├── CMakeLists.txt          build configuration (commented)
├── DEVELOPER_GUIDE.md      this file
├── include/                C++ headers (.hpp) – same structure as src/
├── src/                    C++ sources (.cpp)
│   ├── main.cpp            entry point + ARCHITECTURE OVERVIEW comment (read it!)
│   ├── Telemetry/
│   │   ├── data_acquisition/   protocol definition and raw reading
│   │   ├── Services/           services (serial, file loading, ...) + PacketParser.cpp
│   │   ├── data_writing/       statistics, distribution of rows, CSV logging
│   │   └── file_reading/       in-memory dataset and playback
│   ├── UI/                 one class per window
│   ├── application_services/   user settings (JSON)
│   └── utils/              per-OS resources (fonts, images) and paths
├── assets/                 fonts, images, default window layout, Windows resources
├── scripts/                gen_amk_diagnostics.py (regenerates the AMK diagnostic table)
├── tester_uart/            Python ECU simulator over a serial port (GUI)
├── tester_bin_sd/          Python generator of a test .bin file
└── external/               third-party libraries (do not edit)
```

### The files you will touch most often

| File | What it is | When you edit it |
|---|---|---|
| `src/Telemetry/data_acquisition/TelemetrySchema.cpp` | **The list of every field of packets A and B** (name, type, scale, decimals) | Every time a value is added, changed or removed in the firmware |
| `include/Telemetry/data_acquisition/TelemetrySchema.hpp` | Types used by the schema + the rules for changing it | Rarely (new text formats) |
| `src/UI/*Window.cpp` | The windows | To show a value in a specific window |
| `src/UI/UIManager.cpp` / `include/UI/UIManager.hpp` | Creates and draws all windows | To add/remove a whole window |
| `tester_uart/data_gen.py` and `tester_bin_sd/main.py` | Python packet generators | Every time the packet format changes |

Other important classes, all documented in their headers:

| Class | Role |
|---|---|
| `PacketParser` | decodes the live byte stream into rows |
| `SerialService` / `SerialDataSource` | serial port reading (two threads) |
| `DataAggregator` | adds statistics (mean/variance, max/mean, distance) to every live row |
| `DataManager` | sends rows to the windows (10 Hz) and to the CSV logger (200 Hz) |
| `FileService` | loads `.bin` / `.csv` files |
| `PlaybackManager` | playback cursor, plot data, high-res CSV export |
| `ServiceManager` | static registry that owns all the services |

### How data flows

```
LIVE:   ECU → serial → SerialService → PacketParser → DataAggregator → DataManager → windows + CSV
FILE:   .bin/.csv → FileService → TelemetryDataset → PlaybackManager → windows
```

A row of data is a `DbRow`, i.e. a `std::map<std::string, std::string>`: **the key is the field name defined in `TelemetrySchema.cpp`** (e.g. `"steer"`), the value is the already formatted text (e.g. `"12.5"`). Windows read values by name, so the field name is the link between backend and UI.

---

## 5. Building and running

### Windows (MinGW)

From the repository root, in any terminal (PowerShell, Git Bash, VS Code terminal):

1. **Configure** (only the first time, or after adding a new source folder / editing `CMakeLists.txt`):

```bash
cmake -S . -B build_windows -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```

2. **Build** (every time you change the code):

```bash
cmake --build build_windows -j 8
```

3. **Run**:

```bash
./build_windows/telemetry.exe
```

The MinGW build has no console window: all the messages (`std::cout` / `std::cerr`) appear in the **Log terminal** window inside the application. Lines starting with `ERROR` are red, `WARNING` orange, `LOGGING` purple.

A clean rebuild from scratch: delete the `build_windows` folder and repeat steps 1 and 2.

> VS Code: the `.vscode/settings.json` file configures the *CMake Tools* extension to use the Ninja generator in `build/`. If you do not have Ninja installed, either install it (`pacman -S mingw-w64-x86_64-ninja` in MSYS2) or simply use the terminal commands above. **Ctrl+Shift+B** runs `cmake --build build`.

### Linux

```bash
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build_linux -j 8
```

```bash
./build_linux/telemetry
```

To open serial ports without `sudo`, add your user to the `dialout` group (`sudo usermod -aG dialout $USER`, then log out and back in).

### macOS

```bash
cmake -S . -B build_macos -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build_macos -j 8
```

```bash
open build_macos/telemetry.app
```

### Files the application creates

| What | Where |
|---|---|
| CSV logs (live) and CSV exports | `output_data/` next to the executable (on macOS next to the `.app`) |
| User settings (`settings/settings.json`) and window layout (`imgui.ini`) | Windows `%APPDATA%\telemetry_manager`, Linux `~/.config/telemetry_manager`, macOS `~/Library/Application Support/telemetry_manager` |

To reset the window layout to the default one, close the app and delete `imgui.ini` in that folder, then set `"firstAccess": true` in `settings/settings.json`.

---

## 6. Testing without the car

### 6.1 With a `.bin` file (simplest, no serial port needed)

1. Generate 10 seconds of random data:

```bash
python tester_bin_sd/main.py
```

   This creates `tester_bin_sd/tester_bin.bin`.

2. Start the application, wait for the splash screen to disappear.
3. In the **Load from file** window press **Browse**, go to the `tester_bin_sd` folder (use the path bar or the drive buttons), select `tester_bin.bin`, press **Open**, then **Load and analyze**.
4. All the windows fill with data. Move through time with the **Playback control** slider.
5. In the **Log terminal** check the line `INFO [FileService]: BIN decoded with layout A/current / B/current - 2000 rows.` – `A/current` means the file matches the current packet layout.
6. (Optional) In **Data management** press **Generate High-Res CSV**: the CSV is written to `output_data/`. Go back **Home** (menu bar) and load that CSV to check that the values are the same.

### 6.2 Live, with the serial simulator

You need two serial ports connected to each other:

- **Windows**: install [com0com](https://sourceforge.net/projects/com0com/), which creates a pair of virtual ports (e.g. `COM5` ↔ `COM6`).
- **Linux**: `sudo socat -d -d pty,raw,echo=0,link=/dev/ttyS20 pty,raw,echo=0,link=/dev/ttyS21` (the `ttyS` names make them appear in the app list).
- **Any OS**: two USB-serial adapters with TX/RX crossed and GND connected.

Then:

1. Start the simulator:

```bash
python tester_uart/main.py
```

2. In the simulator window choose the first port (e.g. `COM5`), baud `500000`, press **START SPAM**.
3. In the application, **Configure serial connection** window: choose the other port (e.g. `COM6`), baud `500000`, press **Connect**.
4. Data appears live. In the **Log terminal** you should see `Locked on packet layout A/current (140 bytes)`.
5. In **Data management** press **Start logging** to record a CSV in `output_data/`, then **Stop logging**.

---

## 7. Changing the packets

**Golden rule: the packet layout is described in ONE place only, `src/Telemetry/data_acquisition/TelemetrySchema.cpp`.** The live parser, the `.bin` loader, the CSV logger, the CSV loader, the export and the Dashboard table are all generated from it.

The order of the fields in the C++ table **must be exactly the order in which the firmware sends them**, with the same binary types. If the two do not match, the CRC still validates but the values end up in the wrong fields, or (if the size differs) the log shows `Packet 0xCC corrupted ... no known layout validates its CRC`.

### Field definition format

Each field is one line:

```cpp
{ "name", WireType::U16, ValueFormat::Number, scale, bias, decimals },
```

| Part | Meaning |
|---|---|
| `"name"` | Unique name. It becomes the CSV column name and the key used by the windows. |
| `WireType` | Binary type sent by the firmware: `U8` (uint8_t), `U16` (uint16_t), `I16` (int16_t), `U32` (uint32_t), `F32` (float). |
| `ValueFormat` | How it is shown: `Number` for normal numbers; `EmmaError`, `InverterFsm`, `TractiveFsm`, `EcuMode`, `InverterDiag` for codes shown as names. |
| `scale`, `bias` | Shown value = raw × scale + bias. E.g. a temperature sent ×10 uses `0.1, 0.0`. |
| `decimals` | Decimal digits shown and written to the CSV. |

### Why there are "frozen" layouts

Old recordings (`.bin` files on the SD cards) were written with older firmware. To keep them readable, `TelemetrySchema.cpp` keeps every past layout of packet A (`fieldsA_v1()` … `fieldsA_v4()`) and of packet B (`fieldsB_v1()`). When a file or a serial stream is decoded, the software tries the layouts from the newest to the oldest and picks the one whose length and CRC match.

- `fieldsA_current()` / `fieldsB_current()` = **what the ECU sends today**. This is what you edit.
- `fieldsA_vN()` / `fieldsB_vN()` = **frozen**. Never edit them.

**Before changing `fieldsA_current()`**, ask yourself: *has the current layout already been used to record data that we want to keep?*

- If **no**, just edit `fieldsA_current()`.
- If **yes**, first create a frozen copy of it (step 7.1.1 below).

> Today `fieldsA_current()` is identical to the frozen `fieldsA_v4()` and `fieldsB_current()` has no frozen copy yet. So: to change packet A now you can edit `fieldsA_current()` directly (v4 already protects today's recordings); to change packet B, first freeze the current B layout as `fieldsB_v2()`.

### 7.1 Adding a value

Example: the firmware now sends a brake line pressure as a `uint16_t` multiplied by 10, right after `inv_R_errorInfo` at the end of packet A.

**7.1.1 – (only if needed) freeze the current layout.** In `TelemetrySchema.cpp`, add a function that returns a copy of the current layout *as it is before your change*:

```cpp
static std::vector<FieldDef> fieldsA_v5() {
    std::vector<FieldDef> fields = fieldsA_v4();
    // ... the same changes that fieldsA_current() had at this moment ...
    return fields;
}
```

and register it in `TelemetrySchema::layoutsA()`, **right below** `"A/current"`:

```cpp
addLayout(list, "A/current",            0xCC, 0x11, fieldsA_current());
addLayout(list, "A/v5 (...)",           0xCC, 0x11, fieldsA_v5());   // new
addLayout(list, "A/v4 (inverter diag)", 0xCC, 0x11, fieldsA_v4());
```

**7.1.2 – add the field to the current layout**, in the position the firmware sends it (here: at the end):

```cpp
static std::vector<FieldDef> fieldsA_current() {
    std::vector<FieldDef> fields = fieldsA_v3();
    fields.push_back({ "inv_L_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });
    fields.push_back({ "inv_R_errorInfo", WireType::U16, ValueFormat::InverterDiag, 1.0, 0.0, 0 });
    fields.push_back({ "brake_line_pressure", WireType::U16, ValueFormat::Number, 0.1, 0.0, 1 });  // new
    return fields;
}
```

For a field in the **middle** of the packet, insert it at the right index (see how `fieldsA_v1()` uses `fields.insert(...)`).

**7.1.3 – (optional) statistics.** To get mean/variance/standard deviation over 3 s windows add the name to `varianceChannels()`; for session max/mean add it to `maxMeanChannels()` (both in `TelemetrySchema.cpp`). The **Sensor statistics** window shows them automatically. For a readable label and unit in that window add an entry to `displayNameFor()` in `src/UI/VarianceWindow.cpp`.

**7.1.4 – build.** At this point the new value already appears in:
- the **Current data** table of the **Data management** window;
- the CSV log, the `.bin` loader, the CSV loader and the export.

**7.1.5 – (optional) show it in a dedicated window.** Open the window where it belongs, e.g. `src/UI/DynamicsPowertrainWindow.cpp`, and inside `draw()` add one line where you want it:

```cpp
printValue("Brake line pressure", "brake_line_pressure", dataToDisplay, "bar");
```

The second argument is the field name from step 7.1.2. `InverterDataWindow` and `ModulesBatteryWindow` have the same `printValue` helper; `InverterDataWindow` also has `printColoredValue` (green/orange/red thresholds).

To add it to a **plot**, e.g. in `src/UI/TemperatureWindow.cpp`, add the pair `{ "key", "Legend label" }` to the `PlotGraph` in the constructor **and** the key to `keys_to_plot`:

```cpp
m_temperaturePlot("Temperatures", {{ "inv_L_tempMotor", "Left motor" },
                                   /* ... */
                                   { "brake_line_pressure", "Brake line" }}, 0.0, 120.0)
...
const std::vector<std::string> keys_to_plot = { /* ... */, "brake_line_pressure" };
```

To create a **whole new window**, follow the checklist at the top of `include/UI/UIManager.hpp` ("How to add a NEW data window").

**7.1.6 – update the Python simulators** (section 8), then test (section 6).

### 7.2 Changing a value

| Change | What to do |
|---|---|
| **Name** | Freeze if needed, rename in `fieldsA_current()`, then search the old name in `src/` (VS Code: Ctrl+Shift+F) and update every window that uses it. Old CSV files keep the old column name: they still load, the column is shown as an unknown channel. |
| **Type or position** (the bytes on the wire change) | Freeze the current layout first (7.1.1), then edit `fieldsA_current()`. Update the Python simulators. |
| **Scale, bias or decimals only** | Edit `fieldsA_current()`. Note: since the bytes on the wire do not change, the software cannot tell old and new recordings apart, so the new scale is applied to old `.bin` files too (old CSV files are not affected, they contain already-converted values). |
| **Shown as a name instead of a number** | Use an existing `ValueFormat`, or add a new one: enum value in `TelemetrySchema.hpp`, name table + branches in `formatValue()` and `parseValue()` in `TelemetrySchema.cpp`. |

### 7.3 Removing a value

1. Freeze the current layout if it has been used for recordings (7.1.1).
2. Remove the line from `fieldsA_current()` (or `fieldsB_current()`).
3. Remove the name from `varianceChannels()` / `maxMeanChannels()` if it is there.
4. Search the name in `src/UI` and remove/update the windows that use it (if you forget one, it simply shows `Null` or `--`, it does not crash).
5. Update the Python simulators (section 8).

### 7.4 Packet B (battery)

Exactly the same procedure, using `fieldsB_current()`, `fieldsB_vN()` and `layoutsB()`. In the simulators the function to change is `generate_packet_B()`.

### 7.5 Checklist after any packet change

- [ ] `TelemetrySchema.cpp` edited (and frozen layout added if needed)
- [ ] windows updated (search the field name in `src/UI`)
- [ ] `tester_uart/data_gen.py` **and** `tester_bin_sd/main.py` updated
- [ ] build succeeds
- [ ] `.bin` test (6.1): the log says `layout A/current`, the new value shows in **Current data**
- [ ] live test (6.2): the log says `Locked on packet layout A/current (N bytes)` with the expected size
- [ ] an old `.bin` recording still loads

---

## 8. Updating and running the Python simulators

`tester_uart/data_gen.py` (used by the serial simulator) and `tester_bin_sd/main.py` (file generator) contain **the same two functions**, `generate_packet_A()` and `generate_packet_B()`. Always change both.

Each function:
1. generates a random value per field;
2. packs them with `struct.pack(fmt, ...)` in the packet order;
3. appends the CRC.

The `fmt` string describes the binary types, in the same order as `TelemetrySchema.cpp`. Mapping between the C++ `WireType` and the Python `struct` codes:

| C++ `WireType` | Python `struct` code |
|---|---|
| `U8` | `B` |
| `U16` | `H` |
| `I16` | `h` |
| `U32` | `I` |
| `F32` | `f` |

A number before a code repeats it (`4f` = four floats). `<` at the start means little-endian. The fixed header is `BB I` (two start bytes + uint32 timestamp).

### Example: adding `brake_line_pressure` (uint16 ×10) at the end of packet A

In `generate_packet_A()` of **both** files:

1. Generate the raw value (remember the scale: the C++ side multiplies by 0.1, so send the value ×10):

```python
brake_pressure = random.randint(0, 1500)   # 0.0 .. 150.0 bar
```

2. Add one `H` at the end of `fmt`:

```python
fmt = '< BB I 4f 2f 2H f 2H f 4B 3f H 11f 6h 2f 4B H 2H H'
```

3. Add the variable at the end of the `struct.pack(...)` arguments, after `inv_L_diag, inv_R_diag`:

```python
        imd_insulation,
        inv_L_diag, inv_R_diag,
        brake_pressure
    )
```

### Checking the packet size

The size printed in the application log (`Locked on packet layout A/current (N bytes)`) must match the Python packet size:

```bash
python -B -c "import sys; sys.path.insert(0, 'tester_uart'); import data_gen; print(len(data_gen.generate_packet_A(0)), len(data_gen.generate_packet_B(0)))"
```

Today it prints `140 81`. Every `U8` adds 1 byte, `U16`/`I16` 2 bytes, `U32`/`F32` 4 bytes.

### Running them

```bash
python tester_bin_sd/main.py
```

creates `tester_bin_sd/tester_bin.bin` (load it as in 6.1).

```bash
python tester_uart/main.py
```

opens the serial simulator (use it as in 6.2).

---

## 9. Other maintenance tasks

| Task | How |
|---|---|
| **New user setting** | See the comment at the top of `include/application_services/SettingsManager.hpp`. |
| **New image or font** | See the comment at the top of `include/utils/IAssetManager.hpp` (it must be registered for Windows, macOS and Linux). |
| **Update the AMK inverter diagnostic list** | `python scripts/gen_amk_diagnostics.py` (needs internet). It regenerates `AmkDiagnostics.hpp/.cpp`: never edit those two files by hand, edit the script instead. |
| **Add a new source folder** | Add it to `SRC_FILES` in `CMakeLists.txt` and re-run the configure command. New files in existing folders are picked up automatically. |

---

## 10. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| `fatal error: imgui.h: No such file or directory` | Submodules not downloaded: run the `git submodule update --init ...` command of section 3. |
| CMake fails while fetching `jsoncons` | No internet connection (or a proxy/SSL problem) during the first configure. If you already have a copy of the jsoncons sources (e.g. `build_windows/_deps/jsoncons-src` from a previous build, or a manual `git clone --branch v0.177.0 https://github.com/danielaparker/jsoncons.git`), add `-DFETCHCONTENT_SOURCE_DIR_JSONCONS=<path-to-jsoncons>` to the configure command. |
| `g++` / `mingw32-make` not found | `C:\msys64\mingw64\bin` is not in `PATH`. |
| No port in **Configure serial connection** | Press **Refresh ports**; check the cable/driver; on Linux check the `dialout` group. |
| Log shows `no known layout validates its CRC` | Firmware and `TelemetrySchema.cpp` disagree on the packet layout (field missing, wrong type, wrong order) or the baud rate is wrong. |
| Log shows `bytes of garbage before the current packet` occasionally | Normal after connecting mid-stream or on a noisy link; frequent messages mean a wrong baud rate or bad wiring. |
| A window shows `Null` or `--` | The field name used in the window does not exist in the current data (renamed/removed field, or an old recording without that field). |
| Many lost packets | The ECU or the link cannot keep up; check the counters in **Data management**. |
| Windows are in strange positions | Delete `imgui.ini` (see the table in section 5). |
