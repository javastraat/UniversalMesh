import requests

def send_to_mesh(ip_address, text_message):
    url = f"http://{ip_address}/api/tx"
    
    # Automatically convert the human text into a Hex string
    hex_payload = text_message.encode('utf-8').hex().upper()
    
    data = {
        "dest": "FF:FF:FF:FF:FF:FF",
        "appId": 1,  # Tells the Gateway to decode it back to text
        "ttl": 4,
        "payload": hex_payload
    }

    try:
        response = requests.post(url, json=data, timeout=5)
        print(f"Status: {response.json()}")
    except Exception as e:
        print(f"Failed to reach Master: {e}")

# Fire a message into the mesh
send_to_mesh("192.168.12.154", "Mesh is online!")