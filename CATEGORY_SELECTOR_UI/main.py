from flask import Flask, request, jsonify
from flask_cors import CORS
import requests
import os
import time

app = Flask(__name__)
CORS(app)

# ----------------------------
# TELEGRAM CONFIG
# ----------------------------
TELEGRAM_TOKEN = os.environ.get("TELEGRAM_TOKEN", "your_token")
TELEGRAM_CHAT_ID = os.environ.get("TELEGRAM_CHAT_ID", "your_chat_id")

# ----------------------------
# MODEL ENDPOINTS
# ----------------------------
MODEL_ENDPOINTS = {
    "fruit": "https://fruit-shelflife-hab9a5engxa3hsdv.eastus2-01.azurewebsites.net/predict",
    "meat": "https://shelflife-e3cmc8asfjh9hph4.eastus2-01.azurewebsites.net/predict",
    "dairy": "https://ds-shelf-life-dkdxfgbeebahh4ad.eastus2-01.azurewebsites.net/predict"
}

PERSIST_FILE = "current_category.txt"

# ==============================================================
#  CATEGORY MEMORY + ENV-PERSIST SYSTEM
# ==============================================================
def read_persisted_category():
    # 1️⃣  Check environment variable
    cat = os.environ.get("ACTIVE_CATEGORY")
    if cat:
        print(f"[BOOT] Loaded category from ENV: {cat}")
        return cat
    # 2️⃣  Fallback to file if ENV empty
    try:
        if os.path.exists(PERSIST_FILE):
            with open(PERSIST_FILE, "r") as f:
                cat = f.read().strip()
                if cat:
                    print(f"[BOOT] Loaded saved category: {cat}")
                    return cat
    except Exception as e:
        print("[WARN] Could not read persisted category:", e)
    return None


def write_persisted_category(cat):
    try:
        # Save to ENV (survives Render sleep)
        os.environ["ACTIVE_CATEGORY"] = cat
        # Also mirror locally (for redundancy)
        with open(PERSIST_FILE, "w") as f:
            f.write(cat)
        print(f"[SAVE] Category '{cat}' persisted to ENV + file.")
    except Exception as e:
        print("[ERROR] Could not persist category:", e)


ACTIVE = {"value": read_persisted_category()}


# ==============================================================
#  ROUTES
# ==============================================================

@app.route("/")
def home():
    return (
        "<h3>IoT-Enabled Perishable Food Quality Cloud Router Active</h3>"
        "<p>Use /set_category, /get_category, or /data</p>"
    )


@app.route("/get_category")
def get_category():
    val = ACTIVE.get("value") or read_persisted_category()
    if val:
        return jsonify({"category": val})
    else:
        return jsonify({"error": "No active category set"}), 404


@app.route("/set_category", methods=["POST"])
def set_category():
    body = request.get_json(force=True)
    new_cat = (body.get("category") or "").strip().lower()
    if not new_cat:
        return jsonify({"error": "Category missing"}), 400

    old_cat = ACTIVE.get("value") or read_persisted_category()
    if old_cat != new_cat:
        ACTIVE["value"] = new_cat
        write_persisted_category(new_cat)

        # --- Telegram notice once per change ---
        try:
            msg = f"🌀 Category changed to *{new_cat.capitalize()}* — Model Active"
            telegram_url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
            payload = {"chat_id": TELEGRAM_CHAT_ID, "text": msg, "parse_mode": "Markdown"}
            requests.post(telegram_url, json=payload, timeout=5)
        except Exception as e:
            print("[WARN] Telegram alert failed:", e)

    print(f"[UPDATE] Active category set to: {new_cat}")
    return jsonify({"category": new_cat})


@app.route("/data", methods=["POST"])
def data_in():
    """ESP/Python sends data here → router forwards to correct model"""
    try:
        data = request.get_json(force=True)
    except Exception:
        return jsonify({"error": "Invalid or missing JSON"}), 400

    cat = ACTIVE.get("value") or read_persisted_category()
    if not cat:
        return jsonify({"error": "No active category set"}), 400

    model_url = MODEL_ENDPOINTS.get(cat)
    if not model_url:
        return jsonify({"error": f"No model endpoint found for '{cat}'"}), 500

    print(f"➡️ Forwarding data to {cat} model: {model_url}")
    print(" Payload:", data)

    try:
        response = requests.post(model_url, json=data, timeout=60)
        print(f"✅ Response {response.status_code} from {cat} model")

        try:
            model_result = response.json()
        except Exception:
            model_result = {"text": response.text}

        # --- One-time Telegram alert when 'spoiled' appears ---
        if isinstance(model_result, dict):
            text_content = str(model_result).lower()
            if "spoil" in text_content or "bad" in text_content:
                # avoid repeated alerts
                last_alert_file = f"last_alert_{cat}.txt"
                last_alert = ""
                if os.path.exists(last_alert_file):
                    last_alert = open(last_alert_file).read().strip()
                if last_alert != "sent":
                    alert_msg = f"❌ ALERT({cat.capitalize()}): FOOD SPOILED! Dispose immediately."
                    telegram_url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
                    payload = {"chat_id": TELEGRAM_CHAT_ID, "text": alert_msg}
                    try:
                        requests.post(telegram_url, json=payload, timeout=5)
                        open(last_alert_file, "w").write("sent")
                    except Exception as e:
                        print("[WARN] Telegram alert send failed:", e)

        return jsonify(model_result), response.status_code

    except Exception as e:
        print("❌ Forward error:", e)
        return jsonify({"error": "Failed to contact model", "detail": str(e)}), 502


# Keep memory refreshed on each request
@app.before_request
def refresh_category():
    env_cat = os.environ.get("ACTIVE_CATEGORY")
    if env_cat and ACTIVE.get("value") != env_cat:
        ACTIVE["value"] = env_cat
        print(f"[REFRESH] Category reloaded from ENV: {env_cat}")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 8000)))
