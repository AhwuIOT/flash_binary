## 📦 Tutorial: ESP32 Preparation & Flashing for Remote `.bin` Download

- **Board:** ESP32-DOIT Development Board
- **OS Environment:** Windows 11
- **ESP-IDF Version:** 5.4
- **Flash Size Requirement:** **Serial flash must be set to 4MB**

---

### 🧪 Flashing and Setup Steps (ESP-IDF v5.4)

1. **Set the ESP32 target**

   ```bash
   idf.py set-target esp32
   ```

2. **Open `menuconfig` to configure flash size and use a custom partition table**

   ```bash
   idf.py menuconfig
   ```

   In the configuration menu:

   ```
   → Serial flasher config
       → Flash size: "4MB"            ✅ (Must be 4MB or larger)

   → Partition Table
       → Partition Table: "Custom partition table CSV" ✅
       → Custom partition CSV file: "partitions.csv"
   ```

   > 🛠 **Important:** You **must** set the Partition Table mode to **"Custom partition table CSV"**, and specify the correct filename (`partitions.csv`) to match the custom layout below.

3. **Use the following `partition.csv` file in your project root**

   ```csv
   # Name,      Type, SubType, Offset,   Size
   nvs,         data, nvs,     0x9000,   0x6000
   phy_init,    data, phy,     0xf000,   0x1000
   factory,     app,  factory, 0x10000,  0x140000
   binary,      data, 0x40,    0x150000, 0x100000
   ```

   > ⚠️ This layout requires at least **4MB flash size**, since the `binary` partition starts at `0x150000` and occupies 1MB (`0x100000`).

4. **Build the project**

   ```bash
   idf.py build
   ```

5. **Flash the project to the ESP32-DOIT board**

   ```bash
   idf.py flash monitor
   ```


6. **(⚠️ Note on VSCode + ESP-IDF Extension Flashing Issues)**

    > Many users have reported that **flashing via the VSCode ESP-IDF extension** occasionally fails, especially on Windows systems with certain USB drivers or high-speed baud rates.

    Symptoms include:

    * Upload process stuck at "Connecting..."
    * Error: `Timed out waiting for packet header`
    * Flashing fails silently, or binary is not executed

    🛠️ **Recommendation:** Use a standalone terminal to flash manually with `esptool.py`.


7. **✅ Recommended: Manually flash your compiled binary**

    Once your firmware (`flash_binary.bin`) is built successfully using `idf.py build`, flash it manually:

    ```bash
    esptool.py -p COM4 -b 460800 write_flash 0x10000 flash_binary.bin
    ```

    ✅ Parameters explained:

    * `-p COM4`: Replace with the actual COM port of your ESP32
    * `-b 460800`: Flash baud rate (you may try 115200 if unstable)
    * `0x10000`: Offset for the `factory` app partition
    * `flash_binary.bin`: Path to the built binary (usually in `build/` folder)

    > 💡 Tip: Make sure your ESP32-DOIT board is in **bootloader mode** when flashing.
    > You can hold the **BOOT** button while pressing **EN** (reset), then release **EN**, and finally release **BOOT**.


    ### ✅ Checklist for Success

    * Flash size set to **4MB** in `menuconfig`
    * Partition Table set to **Custom CSV**
    * Flashing via `esptool.py` completes without errors
    * ESP32-DOIT boots and connects to Wi-Fi
    * Binary is downloaded and written to `binary` partition
    * Serial monitor shows 64-byte hex dump for verification



8. **Monitor the serial output**
   You should see logs like:

   ```
    I (5566) bin_downloader: 📥 Written 61440 bytes...
    I (5706) bin_downloader: 📥 Written 65536 bytes...
    I (5706) bin_downloader: ✅ HTTP download complete
    I (5716) bin_downloader: ✅ Done. Total downloaded: 65536 bytes
    I (5716) bin_downloader: 🔍 Verify: Read back first 64 bytes:
    I (5716) bin_downloader: 0x3ffba9c8   aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa  |................|
    I (5726) bin_downloader: 0x3ffba9d8   aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa  |................|
    I (5736) bin_downloader: 0x3ffba9e8   aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa  |................|
    I (5746) bin_downloader: 0x3ffba9f8   aa aa aa aa aa aa aa aa  aa aa aa aa aa aa aa aa  |................|
   ```

