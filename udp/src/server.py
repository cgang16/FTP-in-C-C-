import socket

size = 8192
msgnum = 1

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', 9876))

try:
  while True:
    data, address = sock.recvfrom(size)
    data = str(msgnum) + ' ' + data
    sock.sendto(data, address)
    msgnum = msgnum + 1
finally:
  sock.close()