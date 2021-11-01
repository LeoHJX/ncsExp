#!/usr/bin/env python3

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = '127.0.0.1'
server_port = 65003 #linstin this port

server = (server_address, server_port)
sock.bind(server)
print("Listening on ", server_address, ":", str(server_port), flush=True)

while True:
    payload, client_address = sock.recvfrom(4096)
    print("Echoing back to ", str(client_address), ": ", payload)
    sent = sock.sendto(payload, client_address)
    print("Sent len: ", str(sent), flush=True)
