from flask import Flask, send_from_directory
import os

app = Flask(__name__)

# 設定 firmware 目錄路徑
FIRMWARE_DIR = os.path.join(os.path.dirname(__file__), "firmware")

@app.route("/firmware/<filename>")
def download_firmware(filename):
    return send_from_directory(FIRMWARE_DIR, filename, as_attachment=True)

if __name__ == "__main__":
    app.run()
