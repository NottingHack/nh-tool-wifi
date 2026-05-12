import paho.mqtt.client as mqtt

tool = "test"

# Incoming topics
AUTH =     f"nh/tools/{tool}/AUTH"
COMPLETE = f"nh/tools/{tool}/COMPLETE"
INDUCT =   f"nh/tools/{tool}/INDUCT"
RESET =    f"nh/tools/{tool}/RESET"

# Outgoing topics
GRANT =    f"nh/tools/{tool}/GRANT"
DENY =     f"nh/tools/{tool}/DENY"
ISUC =     f"nh/tools/{tool}/ISEC"
IFAL =     f"nh/tools/{tool}/IFAL"

# Cards
cards = {
    "AAAAAAAAAAAAAAAA": 'U',
    "BBBBBBBBBBBBBBBB": 'M',
    "CCCCCCCCCCCCCCCC": 'I',
}

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(AUTH)
    client.subscribe(COMPLETE)
    client.subscribe(INDUCT)
    client.subscribe(RESET)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    print(msg.topic+" "+str(msg.payload))

    if msg.topic == AUTH:
        card = cards.get(msg.payload.decode(), None)
        if card:
            client.publish(GRANT, card + "Top Line\nBottom Line")
            print("GRANTED for " + card)
        else:
            client.publish(DENY, "Access Denied")
            print("DENIED")
    elif msg.topic == COMPLETE:
        print("COMPLETE")
    elif msg.topic == INDUCT:
        inductor, inductee = msg.payload.decode().split(':')
        card = cards.get(inductor, None)
        if card == 'I':
            client.publish(ISUC, "")
            print("INDUCTED " + inductee)
        else:
            client.publish(IFAL, "Did Not Induct")
            print("NOT INDUCTED " + inductee)
    elif msg.topic == RESET:
        print("RESET " + msg.payload)

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect("192.168.0.59", 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
mqttc.loop_forever()

