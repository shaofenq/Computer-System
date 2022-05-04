import socket
import datetime
from _thread import *
import json
import logging

with open('ip.json') as f:
    ip = json.load(f)

with open('port.json') as fp:
    port = json.load(fp)

state = ['null', 'null', 'null']
server_name = 'S3'
server_ip = ip[server_name]
server_port = port[server_name]

status = ['died', 'died', 'died']

def init_socket(server_ip, server_port):
    # Create a TCP/IP socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Bind the socket to the port
    server_address = (server_ip, server_port)
    print("Starting up on {} port {}".format(server_address[0], server_address[1]))
    server.bind(server_address)

    # Listen for incoming connections
    server.listen(5)
    print("Waiting for connection!")
    return server

def threaded_client(connection, server_name):

    # receive client name: C1 / C2 / C3 from clients
    client_name = connection.recv(1024).decode()

    # send success message to notify client/LFD of successful connection
    success_message = ("Successful connection between {} and {}!".format(server_name, client_name))
    connection.send(success_message.encode())
    
    # print success message to server console
    print(success_message)


    # receive and print message from client / LFD
    current_time = datetime.datetime.now()
    request_message = connection.recv(1024).decode()
    print("[{}] Received <{}, {}, {}, request>".format(str(current_time), client_name, server_name, request_message))

    if(client_name != 'LFD3' and request_message != ''):
        logging.info(f"{client_name} Request: {request_message}")

    global state
    # Change state only for clients
    if (client_name == 'C1'):
        # server state before change
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  before processing <{}, {}, {}, request>".format(str(current_time), state[0], client_name, server_name, request_message))

        # server state after change
        state[0] = request_message
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  after processing <{}, {}, {}, request>".format(str(current_time), state[0], client_name, server_name, request_message))
    if (client_name == 'C2'):
        # server state before change
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  before processing <{}, {}, {}, request>".format(str(current_time), state[1], client_name, server_name, request_message))

        # server state after change
        state[1] = request_message
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  after processing <{}, {}, {}, request>".format(str(current_time), state[1], client_name, server_name, request_message))
    if (client_name == 'C3'):
        # server state before change
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  before processing <{}, {}, {}, request>".format(str(current_time), state[2], client_name, server_name, request_message))

        # server state after change
        state[2] = request_message
        current_time = datetime.datetime.now()
        print("[{}] my_state_S = {}  after processing <{}, {}, {}, request>".format(str(current_time), state[2], client_name, server_name, request_message))


    if client_name == 'RM':
        if request_message == 'checkpoint':
            reply_message = ' '.join(state)
        else:
            reply_list = request_message.split(' ')
            if reply_list[0] == 'status':
                global status
                status = reply_list[1:]
            elif reply_list[0] == 'state':
                state = reply_list[1:]
                reply_message = 'I am ready'  

    
    print(status)

    # send and print receipt message to client/LFD 
    current_time = datetime.datetime.now()
    connection.send(request_message.encode())
    print("[{}] Sending: <{}, {}, {}, reply>".format(str(current_time), client_name, server_name, request_message))

    if(client_name != 'LFD3' and request_message != ''):
        logging.info(f"{client_name} Reply: {request_message}")

    # close the connection and print on server console
    print("Connection to {} closed!\n".format(client_name))
    connection.close()


# Main method

# Initialize a socket
server = init_socket(server_ip, server_port)

#log
FORMAT = '[%(asctime)s] %(message)s'
logging.basicConfig(level=logging.DEBUG,
                    format=FORMAT, datefmt='%Y-%m-%d %H:%M:%S')
file_handler = logging.FileHandler('s3.log', mode='w')
file_handler.setFormatter(logging.Formatter(FORMAT))
logging.getLogger().addHandler(file_handler)

# Run multithreaded socket
while True:
    connection,address = server.accept()
    start_new_thread(threaded_client, (connection, server_name))


