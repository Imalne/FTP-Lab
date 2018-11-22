import socket
import re

host = "127.0.0.1"
port = 21
datahost=""
dataport = 0
data=[]
cport = 0;
port_listen = 0;
port_socket = 0;
data_mode=""


def DownLoadFile(datafd,f,brokeOut):
    while True:
        data = datafd.recv(8192)
        f.write(data)
        if not data:
            break
    f.close()
    datafd.close()

def initLink():
    s = socket.socket()
    s.connect((host, port))
    data = s.recv(1024)
    print(data.decode("utf-8"))
    return s

def get_data_socket():
    global data_socket,port_socket
    if(data_mode == "PASV"):
        data_socket = socket.socket()
        data_socket.connect((datahost, dataport))
    elif(data_mode == "PORT"):
        port_socket, addr = port_listen.accept()




def Handler(Connfd,command,recv):
    global datahost,dataport,data_mode
    print(recv.decode("utf-8"))
    if len(command)<4:
        return
    if(command.upper()[0:4] == "PASV"):
        if(recv.decode("utf-8")[0:3]=="227"):
            group = re.search(r"(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)",recv.decode("utf-8"),re.M)
            if (group):
                datahost = group.group(1)+"."+group.group(2)+"."+group.group(3)+"."+group.group(4)
                dataport = int(group.group(5))*256+int(group.group(6))
                data_mode = "PASV"
    elif command.upper()[0:4] == "PORT" :
        global cport,port_listen
        if(recv.decode("utf-8")[0:3] =="200"):
            info = command[:-2].split(" ")[1].split(',')
            ip=""
            for i in range(0,4):
                ip = ip + info[i]
                if(i<3):
                    ip = ip + '.'
            cport = int(info[4]) * 256 + int(info[5])
            port_listen = socket.socket()
            port_listen.bind((ip, cport))
            port_listen.listen(10)
            data_mode = "PORT"
    elif command.upper()[0:4] == "STOR":
        if data_mode == "PORT":
            if (recv.decode("utf-8")[0:3] == "150"):
                f = open(command.split(" ")[1][0:-2],"rb")
                r = f.read()
                port_socket.send(r)
                f.close()
                # port_socket.shutdown(2)
                port_socket.close()
                # port_listen.shutdown(2)
                port_listen.close()
                print(Connfd.recv(1024).decode("utf-8"))
        elif data_mode == "PASV":

            if (recv.decode("utf-8")[0:3] == "150"):
                f = open(command.split(" ")[1][0:-2],"rb")
                r = f.read()
                data_socket.send(r)
                f.close()
                data_socket.close()
                print(Connfd.recv(1024).decode("utf-8"))
    elif command[0:4].upper() == "RETR":
        if data_mode == "PASV":
            if (recv.decode("utf-8")[0:3] == "150"):
                name = ""
                for i in range(0, len(command)):
                    if (command[len(command) - i - 1] != "/"):
                        pass
                    else:
                        name = command[len(command) - (i):-2]
                        break
                if name == "":
                    name = command[5:-2]
                f = open(name, "wb");
                while True:
                    data = data_socket.recv(8192)
                    f.write(data)
                    if not data:
                        break
                f.close()
                data_socket.close()
                print(Connfd.recv(1024).decode("utf-8"))

        elif data_mode == "PORT":
            if (recv.decode("utf-8")[0:3] == "150"):
                name = ""
                for i in range(0,len(command)):
                    if(command[len(command)-i-1] != "/"):
                        pass
                    else:
                        name=command[len(command)-(i):-2]
                        break
                if name == "":
                    name = command[5:-2]

                f = open(name, "wb");
                while True:
                    data = port_socket.recv(8192)
                    f.write(data)
                    if not data:
                        break
                f.close()
                port_socket.close()
                port_listen.close()
                print(Connfd.recv(1024).decode("utf-8"))
    elif command[0:4].upper() == "LIST":
        if data_mode == "PASV":
            if (recv.decode("utf-8")[0:3] == "150"):
                while True:
                    data = data_socket.recv(8192)
                    print(data.decode("utf-8"))
                    if not data:
                        break
                data_socket.close()
                print(Connfd.recv(1024).decode("utf-8"))

        elif data_mode == "PORT":
            if (recv.decode("utf-8")[0:3] == "150"):
                while True:
                    data = port_socket.recv(8192)
                    print(data.decode("utf-8"))
                    if not data:
                        break
                port_socket.close()
                port_listen.close()
                print(Connfd.recv(1024).decode("utf-8"))
    elif command[0:4].upper() == "QUIT" or command[0:4].upper() == "ABOR":
        if (recv.decode("utf-8")[0:3] == "221"):
            exit()
def tcpClient():
    global port_socket
    s = initLink()
    while True:
        message = input("->")
        if message == 'q':
            break
        if message.upper()[0:4] == "STOR":
            try:
                filepath = message.split()[1]
                fd = open(filepath,"rb")
            except:
                print("please input a filename")
        message = message + '\r' + '\n'
        s.send(message.encode())
        if message.upper()[0:4] == "STOR" or message.upper()[0:4] == "RETR" or message.upper()[0:4] == "LIST":
            try:
                get_data_socket()
            except:
                pass
        data = s.recv(8192)
        Handler(Connfd=s,command=message,recv=data)
    s.close()

if __name__ == "__main__":
    tcpClient()