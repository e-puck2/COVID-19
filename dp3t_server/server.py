import socket
import os
import sys
import threading
import time

MAX_NUM_ROBOTS = 50
PACKET_SIZE_SERVER2ROBOT = (MAX_NUM_ROBOTS*2) # At most all robots id's (2 bytes per robot)
PACKET_SIZE_ROBOT2SERVER = 3 # Only the id of the robot + add(1)|remove(2)

ServerSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host = ''
port = 1000
ThreadCount = 0
infection_list = bytearray(PACKET_SIZE_SERVER2ROBOT)
infection_list_index = 0
infection_list_id = []
mutex = threading.Lock()

try:
	ServerSocket.bind((host, port))	
	ServerSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
except socket.error as e:
	print(str(e))
	
print('Waiting for a Connection..')
ServerSocket.listen(MAX_NUM_ROBOTS)

def threaded_client(connection):
	global ThreadCount
	global infection_list
	global infection_list_index
	global infection_list_id
	
	connection.settimeout(5)
	while True:
		try:
			data = connection.recv(PACKET_SIZE_ROBOT2SERVER)
			if data == b'':
				print("disconnected (rx err)")
				break
			#print(str(data))
			id = data[0]+data[1]*256
			#print(str(id))
			if (id != 0) and (id not in infection_list_id) and (data[2]==1): # New infected robot, add to list
				mutex.acquire()
				infection_list[infection_list_index] = data[0]
				infection_list[infection_list_index+1] = data[1]
				infection_list_index += 2
				infection_list_id.append(id)
				mutex.release()
			elif data[2]==2 and (id in infection_list_id): # Remove robot id from the list				
				curr_ind = infection_list_id.index(id)
				mutex.acquire()
				infection_list_id.remove(id)
				infection_list[curr_ind*2:infection_list_index-2] = infection_list[curr_ind*2+2:infection_list_index]
				infection_list[infection_list_index-1] = 0
				infection_list[infection_list_index-2] = 0
				infection_list_index -= 2
				mutex.release()
			sent = connection.send(infection_list)
			if sent == 0:
				print("disconnected (tx err)")
				break
		except:
			print("Unexpected error:" + str(sys.exc_info()[0]))
			break
	
	ThreadCount -= 1
	connection.shutdown(socket.SHUT_RDWR)
	connection.close()
	print("connection closed with robot")

class TcpServerTask(threading.Thread):
	def __init__(self,):		
		threading.Thread.__init__(self)
	def run(self):	
		global ServerSocket
		global ThreadCount
		while 1:
			Client, address = ServerSocket.accept()
			print('Connected to: ' + address[0] + ':' + str(address[1]))
			threading.Thread(target=threaded_client, args=(Client,)).start()
			ThreadCount += 1
			print('Thread Number: ' + str(ThreadCount))
		ServerSocket.close()

tcp_server_task = TcpServerTask()
tcp_server_task.start()
	
while True:
	if infection_list_index == 0:
		print("No robots infected")
	else:
		my_str = "Infected robots:"
		for i in range(len(infection_list_id)):
			my_str += str(infection_list_id[i]) + ", "
		my_str += "(" + str(len(infection_list_id)) + ")"
		print(my_str)
	time.sleep(1)
