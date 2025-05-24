from flask import Flask, send_file
import os

app = Flask(__name__)

@app.route("/firmware/<filename>")
def download_firmware(filename):
    abs_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "firmware"))
    filepath = os.path.join(abs_dir, filename)

    if not os.path.exists(filepath):
        return "❌ File not found", 404

    return send_file(filepath,
                     mimetype="application/octet-stream",
                     as_attachment=True,
                     download_name=filename)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)