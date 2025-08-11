import paho.mqtt.client as mqtt
import os
import time
import docker
import json
import threading

# --- Configuration ---
MQTT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "mqtt-broker")
MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", 1883))
MQTT_TOPIC_PREFIX = "helium/simulator/ble_write"
MQTT_TOPIC_WILDCARD = f"{MQTT_TOPIC_PREFIX}/#"
ACCOUNT_POOL_FILE = "account_pool.json"

# The BLE characteristic that triggers provisioning a new miner
PROVISIONING_TRIGGER_UUID = "df3b16ca-c985-4da2-a6d2-9b9b9abdb858" # ADD_GATEWAY_CHARACTERISTIC_UUID

# --- Globals for Account Pool ---
account_pool = []
current_account_index = 0
pool_lock = threading.Lock()

# --- Docker Client ---
try:
    docker_client = docker.from_env()
except Exception as e:
    print(f"Error connecting to Docker daemon: {e}")
    docker_client = None

def load_account_pool():
    """Loads accounts from the JSON file."""
    global account_pool
    try:
        with open(ACCOUNT_POOL_FILE, 'r') as f:
            data = json.load(f)
            account_pool = data.get("accounts", [])
            if account_pool:
                print(f"Successfully loaded {len(account_pool)} accounts from {ACCOUNT_POOL_FILE}")
            else:
                print(f"Warning: No accounts found in {ACCOUNT_POOL_FILE}")
    except FileNotFoundError:
        print(f"Error: {ACCOUNT_POOL_FILE} not found. No accounts loaded.")
    except json.JSONDecodeError:
        print(f"Error: Could not decode JSON from {ACCOUNT_POOL_FILE}.")

def get_next_account():
    """Gets the next account from the pool in a thread-safe, round-robin fashion."""
    global current_account_index
    with pool_lock:
        if not account_pool:
            return None

        account = account_pool[current_account_index]
        current_account_index = (current_account_index + 1) % len(account_pool)
        return account

def launch_miner_instance(gateway_data):
    """Launches a new simulated miner container with an account from the pool."""
    if not docker_client:
        print("Docker client not available. Cannot launch miner instance.")
        return

    try:
        # --- ACCOUNT POOL LOGIC ---
        # Get the next account from the pool, ignore the one from the gateway_data
        owner_key = get_next_account()
        if not owner_key:
            print("No accounts available in the pool. Aborting launch.")
            return

        # We still use the payer from the original request, as it might be distinct
        payer_key = gateway_data.get("payer", "unknown_payer")
        container_name = f"sim-miner-{owner_key[:12]}"

        print(f"Attempting to launch miner instance: {container_name}")

        # Check if a container with this name already exists
        try:
            existing_container = docker_client.containers.get(container_name)
            print(f"Container {container_name} already exists. Stopping and removing.")
            existing_container.stop()
            existing_container.remove()
        except docker.errors.NotFound:
            pass # This is the expected case

        # Define environment variables for the container
        environment = {
            'GATEWAY_OWNER': owner_key,
            'GATEWAY_PAYER': payer_key
        }

        command = f'sh -c "echo I am a simulated miner. && echo My owner is $GATEWAY_OWNER && echo My payer is $GATEWAY_PAYER && sleep infinity"'

        container = docker_client.containers.run(
            "alpine:latest",
            command=command,
            name=container_name,
            environment=environment,
            detach=True,
            auto_remove=True
        )
        print(f"Successfully launched container {container.name} ({container.short_id}) with owner {owner_key}")

    except Exception as e:
        print(f"Failed to launch miner instance: {e}")

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(MQTT_TOPIC_WILDCARD)
        print(f"Subscribed to topic: {MQTT_TOPIC_WILDCARD}")
    else:
        print(f"Failed to connect, return code {rc}\n")

def on_message(client, userdata, msg):
    characteristic_uuid = msg.topic.split("/")[-1]
    if characteristic_uuid == PROVISIONING_TRIGGER_UUID:
        print(f"Received provisioning trigger on topic {msg.topic}")
        try:
            payload = msg.payload.decode("utf-8")
            gateway_data = json.loads(payload)
            launch_miner_instance(gateway_data)
        except Exception as e:
            print(f"  Error processing message: {e}")

def main():
    load_account_pool()
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    print(f"Attempting to connect to broker at {MQTT_BROKER_HOST}:{MQTT_BROKER_PORT}")
    while True:
        try:
            client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)
            break
        except ConnectionRefusedError:
            print("Connection refused. Retrying in 5 seconds...")
            time.sleep(5)
        except OSError as e:
            print(f"OS error: {e}. Retrying in 5 seconds...")
            time.sleep(5)
    client.loop_forever()

if __name__ == '__main__':
    main()
