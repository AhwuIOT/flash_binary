# save this as generate_dummy_bin.py
with open("test_firmware.bin", "wb") as f:
    f.write(b'\xAA' * 65536)  # 64KB 固定內容
