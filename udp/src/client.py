import socket

size = 8192

for i in range(0, 51):
    try:
        msg = str(i)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg, ('localhost', 9876))
        print sock.recv(size)
        sock.close()
    except:
        print "cannot reach the server"
