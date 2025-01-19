# S.H.A.D.E. Node

This repository contains the software for a demo Node on the Short-range Hidden Attritable Data Environment. This firmware targets ESP32-C6 but should be compatible with any target supporting Bluetooth LE and the nimBLE stack.

## S.H.A.D.E. Overview

SHADE is designed to provide text communications in a hostile / denied environment where other communications methods that rely on the Internet are blocked or compromised. This is less an instant messaging system, and more a distributed dead drop. 

Our design tenants are:
- Hide in plain sight: use standard BLE Advertisements to transmit data
- Be Quiet: nodes produce no RF emissions when idle
- Be Resilient: SHADE is a many:many designed for redundancy in a pseudo-mesh network
- Be Attritable: No node is required for the operation of any other, nodes should be cheap and easily replaced.

This protocol is not designed to be undetectable, but to be difficult enough to detect any one node that denial of service for the overall network is impractical. 

## Node-to-Node Protocol

### Transmission

A SHADE Node is always configured as a passive BLE Observer, collecting advertisement packets. When a Node has data to send, it will encode this data into a series of BLE advertisements as follows:

- Sender generates a random MAC address that it will use for the duration of this message. This serves as the message ID
- Sender configures itself as a non-connectable, non-scannable, undirected advertiser
- Sender broadcasts a `BT_DATA_NAME_COMPLETE` advertisement with a randomly selected name followed by a base64 checksum of the message payload. Example: `Sony QhZcWbHoHs9UZHK`
- Sender transmits the rest of the message an a series of `BT_DATA_MANUFACTURER_DATA` advertisements with small delays [50-500]ms between transmissions.

Many Manufacturer Data advertisements will likely be required, as the maximum payload per advertisement is 29 bytes. 

This protocol is connection-less by design, meaning that nodes only have to reveal themselves while actively transmitting new data. However, this comes with some drawbacks, each of which are addressed in the following sections:
- Cannot take advantage of BLE security & encryption features
- receiving nodes have no way to ACK / NACK data
- Slow

In this system, cryptography is intentionally left up to the user. By design, nodes have no root of trust as must be easily replaceable, therefore a standard configuration where each node has an asymmetric keyset is not feasible. The same goes for users, where users have no accounts, there is no centralized system to store public keys, and users may not wish to be attributable. 

By leaving encryption to the user, users can post cleartext for anyone to read or encrypt to that only a wanted recipient or recipients can decrypt the message client-side.

### Synchronization

To address the issues of lack of retransmission and to allow nodes to receive messages that were broadcast before the node booted, the protocol has a synchronization function. 

A node will initiate a sync either when it first starts up, or when a received message fails a checksum. 

This is initiated by broadcasting an advertisement with a random MAC address and a `BT_DATA_NAME_SHORTENED` of `SHDSYNC`, optionally followed by a series of `BT_DATA_MANUFACTURER_DATA` advertisements whose payload is a list of MAC addresses (IDs) of messages already known by the requesting node. This indicates to all other nodes that they need not send the messages with these IDs, reducing sync time and visibility. 

When a node receives a sync request, it will wait for a random amount of time [5-2000]ms, listening for any other node to transmit a response. If no response is detected within this time, the node will begin transmitting all messages known to it that were *not* specified in the MAC address list sent by the requesting node. Exponential backoff is not necessary as multiple nodes advertising simultaneously will not cause collisions; this delay is only to reduce discoverability of nodes if a bad actor sends SYNC requests. 

## Node-to-Phone Communication

As nodes spend the majority of their time not advertising, the client phone must act as the BLE peripheral and advertise a GATT service. Using a similar random wait to that described in the previous section as well as an RSSI floor, a random node in range of the phone will initiate a BLE connection, at which point it will use the writable GATT service to send all messages stored onboard the node. The user may then send a message, which is optionally encrypted on-device before being sent to the node via the GATT connection.

After the phone terminates the connection, the node performs another random wait [1-30]minutes before transmitting the user's message to all nodes in range. These downstream nodes will then repeat this until the message has propagated throughout the network. 