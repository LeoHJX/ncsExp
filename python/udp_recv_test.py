#!/usr/bin/env python3

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

server_address = '0.0.0.0'
server_port = 65006 #linstin this port

server = (server_address, server_port)
sock.bind(server)
print("Listening on ", server_address, ":", str(server_port), flush=True)

while True:
    payload, client_address = sock.recvfrom(4096)
    print("packet received: ", str(client_address), "len: ", len(payload), " : ", payload)
#    if(payload.decode("utf-8").find('pkg:') != -1):
#        print("send back to ", str(client_address), ": ", payload[:9])
#        sent = sock.sendto(payload[:160], client_address)
#        print("Sent len: ", str(sent), flush=True)

