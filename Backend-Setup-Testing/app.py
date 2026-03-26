from flask import Flask, jsonify
from flask_cors import CORS
import subprocess

app = Flask(__name__)   # ✅ FIRST define app
CORS(app)               # ✅ THEN apply CORS

@app.route('/test', methods=['GET'])
def test():
    process = subprocess.Popen(
        ['backend.exe'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    output, error = process.communicate()

    return jsonify({"output": output})

if __name__ == '__main__':
    app.run(debug=True)