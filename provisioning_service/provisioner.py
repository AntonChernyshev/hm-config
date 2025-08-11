import paho.mqtt.client as mqtt
import os
import time
import docker
import json

# --- Configuration ---
MQTT_BROKER_HOST = os.getenv("MQTT_BROKER_HOST", "mqtt-broker")
MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", 1883))
MQTT_TOPIC_PREFIX = "helium/simulator/ble_write"
MQTT_TOPIC_WILDCARD = f"{MQTT_TOPIC_PREFIX}/#"

# The BLE characteristic that triggers provisioning a new miner
PROVISIONING_TRIGGER_UUID = "df3b16ca-c985-4da2-a6d2-9b9b9abdb858" # ADD_GATEWAY_CHARACTERISTIC_UUID

# --- Docker Client ---
try:
    docker_client = docker.from_env()
except Exception as e:
    print(f"Error connecting to Docker daemon: {e}")
    docker_client = None

def launch_miner_instance(gateway_data):
    """Launches a new simulated miner container."""
    if not docker_client:
        print("Docker client not available. Cannot launch miner instance.")
        return

    try:
        public_key = gateway_data.get("owner", "unknown_owner")
        container_name = f"sim-miner-{public_key[:12]}" # Create a unique, readable name

        print(f"Attempting to launch miner instance: {container_name}")

        # Check if a container with this name already exists
        try:
            existing_container = docker_client.containers.get(container_name)
            print(f"Container {container_name} already exists. Stopping and removing.")
            existing_container.stop()
            existing_container.remove()
        except docker.errors.NotFound:
            # This is the expected case
            pass

        # Define environment variables for the container
        environment = {
            'GATEWAY_OWNER': gateway_data.get("owner"),
            'GATEWAY_PAYER': gateway_data.get("payer")
        }

        # The command to run inside the placeholder container
        command = f'sh -c "echo I am a simulated miner. && echo My owner is $GATEWAY_OWNER && echo My payer is $GATEWAY_PAYER && sleep infinity"'

        container = docker_client.containers.run(
            "alpine:latest",
            command=command,
            name=container_name,
            environment=environment,
            detach=True,  # Run in the background
            auto_remove=True # Clean up when stopped
        )

        print(f"Successfully launched container {container.name} ({container.short_id})")

    except Exception as e:
        print(f"Failed to launch miner instance: {e}")

def on_connect(client, userdata, flags, rc):
    """The callback for when the client receives a CONNACK response from the server."""
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(MQTT_TOPIC_WILDCARD)
        print(f"Subscribed to topic: {MQTT_TOPIC_WILDCARD}")
    else:
        print(f"Failed to connect, return code {rc}\n")

def on_message(client, userdata, msg):
    """The callback for when a PUBLISH message is received from the server."""
    characteristic_uuid = msg.topic.split("/")[-1]

    # Check if this message is the one that triggers provisioning
    if characteristic_uuid == PROVISIONING_TRIGGER_UUID:
        print(f"Received provisioning trigger on topic {msg.topic}")
        try:
            payload = msg.payload.decode("utf-8")
            print(f"  Payload: {payload}")

            # The payload for add_gateway is expected to be a JSON string
            # like {"owner": "...", "payer": "..."}
            gateway_data = json.loads(payload)
            launch_miner_instance(gateway_data)

        except json.JSONDecodeError:
            print("  Error: Payload is not valid JSON.")
        except Exception as e:
            print(f"  Error processing message: {e}")

def main():
    """Main function to setup and run the MQTT client."""
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
