import wx
import wx.grid
from wx.lib.pubsub import pub
from threading import Thread
import threading
import socket
import re
import os

download_switch = 0
download_len = 0
upload_switch = 0
upload_len = 0

uncomplete_file={}
append_mode = False

in_loading = False



class FTPFrame(wx.Frame):
    def __init__(self):
        self.frame = wx.Frame.__init__(self,None,-1,title="FtpClient",size=(1560,800))
        self.panel = wx.Panel(self, -1,size=(1600,1000))

        self.panel.SetPosition((20,20))
        # connect to server
        Ip_Port_Sizer = wx.BoxSizer(wx.HORIZONTAL)
        IP_Label = wx.StaticText(self.panel,-1,"IP:")
        self.IP_Input = wx.TextCtrl(self.panel)
        Port_Label = wx.StaticText(self.panel,-1,"Port:")
        self.Port_Input = wx.TextCtrl(self.panel)
        self.Connect_Button = wx.Button(self.panel,label="connect")
        Ip_Port_Sizer.Add(IP_Label,0,wx.EXPAND)
        Ip_Port_Sizer.Add(self.IP_Input,1,wx.EXPAND)
        Ip_Port_Sizer.Add(Port_Label,0,wx.EXPAND)
        Ip_Port_Sizer.Add(self.Port_Input,1,wx.EXPAND)
        Ip_Port_Sizer.Add(self.Connect_Button,0,wx.EXPAND)

        # send command
        sizer = wx.BoxSizer(wx.HORIZONTAL)
        self.input_line = wx.TextCtrl(self.panel,style=wx.TE_PROCESS_ENTER)
        self.addButton = wx.Button(self.panel, label="push")
        sizer.Add(self.input_line, 1, wx.EXPAND)
        sizer.Add(self.addButton, 0, wx.EXPAND)
        self.input_line.Enable(False)

        self.history = wx.grid.Grid(self.panel)
        self.history.CreateGrid(0,3)
        self.history.SetColSize(0,180)
        self.history.SetColSize(1,40)
        self.history.SetColSize(2,300)
        self.history.SetColLabelValue(0,"command")
        self.history.SetColLabelValue(1,"code")
        self.history.SetColLabelValue(2,"return message")
        self.history.SetRowLabelSize(0)


        self.list = wx.grid.Grid(self.panel)
        self.list.CreateGrid(0,1)
        self.list.SetColSize(0,520)
        self.list.SetColLabelValue(0,"List")
        self.list.SetRowLabelSize(0)

        self.data_list = wx.grid.Grid(self.panel)
        self.data_list.CreateGrid(0,3)
        self.data_list.SetColSize(0,200)
        self.data_list.SetColSize(1,120)
        self.data_list.SetColSize(2,200)
        self.data_list.SetColLabelValue(0,"data task name")
        self.data_list.SetColLabelValue(1,"progress")
        self.data_list.SetColLabelValue(2,"status")
        self.data_list.SetRowLabelSize(0)
        # self.data_list.Enable(False)

        # grid_sizer.Add(self.history, 1, wx.ALL)
        # grid_sizer.Add(self.list, 1, wx.ALL)
        # layout
        first_line_left = wx.BoxSizer(wx.VERTICAL)
        first_line_left.Add(Ip_Port_Sizer,0,wx.EXPAND)
        first_line_left.Add(sizer,0,wx.EXPAND)
        first_line_left.Add(self.history,2,wx.EXPAND)
        # first_line = wx.BoxSizer(wx.HORIZONTAL)
        # first_line.Add(first_line_left,wx.EXPAND)
        # first_line.Add(self.list,wx.EXPAND)
        mainSizer = wx.GridSizer(cols=3, rows=1, vgap=0,hgap=5)
        # mainSizer.Add(Ip_Port_Sizer,0,wx.SHAPED)
        # mainSizer.Add(sizer,0,wx.SHAPED)
        mainSizer.Add(first_line_left,0,wx.EXPAND)
        mainSizer.Add(self.list,2,wx.EXPAND)
        # mainSizer.Add(self.list,2,wx.EXPAND)
        mainSizer.Add(self.data_list,2,wx.EXPAND)
        # mainSizer.Add(grid_sizer,2,wx.ALL)
        self.panel.SetSizerAndFit(mainSizer)

        # interact event
        self.addButton.Bind(wx.EVT_BUTTON,self.send_button)
        self.Connect_Button.Bind(wx.EVT_BUTTON,self.connect)
        # self.Bind(wx.EVT_CLOSE,self.disconnect)
        self.input_line.Bind(wx.EVT_TEXT_ENTER,self.send_button)
        self.data_list.Bind(wx.grid.EVT_GRID_CELL_LEFT_CLICK,self.data_button)

        self.s = None
        self.port_listen = None
        self.port_socket = None
        self.data_socket = None
        self.data_mode = ""
        self.datahost = ""
        self.dataport = 0
        self.cport = 0;
        self.datahost = ""
        self.dataport = 0

        pub.subscribe(self.updateGauge, "update")
        pub.subscribe(self.set_datalist,"setDataList")
        pub.subscribe(self.showConfdRecv,"showConfdRecv")
        pub.subscribe(self.set_input_status,"setInputStatus")

        self.set_input_status(0);


    def updateGauge(self,msg,length):
        # print(msg)
        self.data_list.SetCellValue(int(msg),1,str(length))

    def set_input_status(self,status):
        global in_loading
        if status == 0:
            self.Port_Input.Enable(True)
            self.IP_Input.Enable(True)
            self.input_line.Enable(False)
            self.addButton.Enable(False)
            in_loading = False
            self.clearHistory()
        elif status == 1:
            self.Port_Input.Enable(False)
            self.IP_Input.Enable(False)
            self.input_line.Enable(True)
            self.addButton.Enable(True)
            in_loading = False
        elif status == 2:
            self.Port_Input.Enable(False)
            self.IP_Input.Enable(False)
            self.input_line.Enable(False)
            self.addButton.Enable(False)
            in_loading = True

    def init_link(self,host,port):
        self.s = socket.socket()
        self.s.connect((host, int(port)))
        data = self.confd_recv()
        self.set_input_status(1)
        print(data)

    def get_data_socket(self):
        if (self.data_mode == "PASV"):
            self.data_socket = socket.socket()
            self.data_socket.connect((self.datahost, self.dataport))
        elif (self.data_mode == "PORT"):
            self.port_socket, addr = self.port_listen.accept()
            self.port_listen.close()

    def list_reshape(self,data):
        data_list = []
        single_line = ""
        for i in range(0,len(data)):
            single_line += data[i]
            if(data[i] == '\n'):
                data_list.append(single_line)
                single_line = ""
        return data_list

    def download(self,socket,filename,index,offset=None,filesize=None):
        global download_switch,append_mode,download_len
        fd = None
        if offset is not None:
            fd = open(filename,"ab")
            download_len = offset
        else:
            fd = open(filename, "wb");
            download_len = 0
        socket.settimeout(1000)
        try:
            while True:
                data = socket.recv(8192)
                # print("get: "+str(download_len)+" bytes")
                download_len += fd.write(data)
                if filesize:
                    wx.CallAfter(pub.sendMessage, "update", msg=index,length=str(int(download_len/filesize*100))+'%')
                    # wx.CallAfter(pub.sendMessage, "update", msg=index,length=download_len)
                else:
                    wx.CallAfter(pub.sendMessage, "update", msg=index,length=download_len)
                if download_switch == 1:
                    download_switch = 0
                    break
                if not data:
                    download_switch = 0
                    download_len = 0
                    break
        except:
            download_len = -1
            download_switch = 0


        socket.close()
        append_mode = False
        fd.close()

    def set_datalist(self,server_path,status):
        for i in range(0,self.data_list.NumberRows):
            if self.data_list.GetCellValue(i,0) == server_path:
                self.data_list.SetCellValue(i,2,status)
                return i
        self.data_list.AppendRows(1)
        self.data_list.SetCellValue(self.data_list.NumberRows - 1, 0, server_path)
        self.data_list.SetCellValue(self.data_list.NumberRows - 1, 2, status)
        self.data_list.SetReadOnly(self.data_list.NumberRows - 1, 0, True)
        self.data_list.SetReadOnly(self.data_list.NumberRows - 1, 1, True)
        self.data_list.SetReadOnly(self.data_list.NumberRows - 1, 2, True)

        return self.data_list.NumberRows - 1

    def download_p(self,socket,filename,server_path,fileSize=None):
        global uncomplete_file,download_len
        index = self.indexInDataList(server_path)
        wx.CallAfter(pub.sendMessage, "setDataList", server_path=server_path, status="press to pause download")


        if append_mode:
            self.download(socket,filename,index,uncomplete_file[server_path],filesize=fileSize)
        else:
            self.download(socket,filename,index,filesize=fileSize)


        if download_len == 0:
            wx.CallAfter(pub.sendMessage, "setDataList", server_path=server_path, status="complete")
            if uncomplete_file.__contains__(server_path):
                try:
                    uncomplete_file.pop(server_path)
                except:
                    pass
        elif download_len == -1:
            uncomplete_file[server_path] = 0
            wx.CallAfter(pub.sendMessage, "setDataList", server_path=server_path, status="press to restart download")
        else:
            uncomplete_file[server_path] = download_len
            download_len = 0
        # if self.confd_recv()[0:3] != "226":
        #     self.set_input_status(1)
        self.confd_recv()
        wx.CallAfter(pub.sendMessage, "setInputStatus",status=1)
        # self.set_input_status(1)


    def continue_download(self,offset,server_path):
        global append_mode,download_len
        if self.con_msg_handler("TYPE I\r\n",show=False):
            if self.con_msg_handler("PASV\r\n",show=False):
                if self.con_msg_handler("REST "+str(offset)+'\r\n',show=False):
                    append_mode = True
                    download_len = offset
                    if self.con_msg_handler("RETR "+server_path+'\r\n',show=False):
                        return True
                    else:
                        print("failed to download")
                        self.set_input_status(1)
                        return False
                else:
                    print("failed to set offset")
            else:
                print("failed to connect")
        else:
            print("failed to set Type")

    def continue_upload(self,offset,filename):
        global append_mode,upload_len
        if self.con_msg_handler("TYPE I\r\n",show=False):
            if self.con_msg_handler("PASV\r\n",show=False):
                if self.con_msg_handler("REST "+str(offset)+'\r\n',show=False):
                    append_mode = True
                    upload_len = offset
                    if self.con_msg_handler("APPE "+filename + '\r\n',show=False):
                        return True
                    else:
                        print("failed to upload")
                        self.set_input_status(1)
                        return False
                else:
                    print("failed to set offset")
            else:
                print("failed to connect")
        else:
            print("failed to set Type")

    def upload(self,socket,filename,index,offset=None,filesize=None):
        global upload_len,upload_switch,append_mode
        f = open(filename, "rb")
        upload_len = 0
        filesize = os.path.getsize(filename)
        if offset is not None:
            f.seek(offset,0)
            upload_len = offset
        while True:
            r = f.read(8192)
            socket.send(r)
            upload_len = upload_len + len(r)
            if filesize:
                wx.CallAfter(pub.sendMessage, "update", msg=index,length=str(int(upload_len/filesize*100))+'%')
            else:
                wx.CallAfter(pub.sendMessage, "update", msg=index,length=upload_len)
            if upload_switch == 1:
                upload_switch = 0
                break
            if len(r) == 0:
                upload_switch = 0
                upload_len = 0
                break
        socket.close()
        append_mode = False
        f.close()


    def upload_p(self,socket,filename):
        global upload_len,uncomplete_file
        index = self.indexInDataList(filename)
        wx.CallAfter(pub.sendMessage, "setDataList", server_path=filename, status="press to pause upload")
        # index = self.set_datalist(filename,"press to pause upload")

        if append_mode:
            self.upload(socket,filename,index,uncomplete_file[filename])
        else:
            self.upload(socket,filename,index)

        if upload_len == 0:
            wx.CallAfter(pub.sendMessage, "setDataList", server_path=filename, status="complete")
            # self.set_datalist(filename,"complete")
            if uncomplete_file.__contains__(filename):
                uncomplete_file.pop(filename)

        else:
            uncomplete_file[filename]=upload_len
            upload_len = 0

        self.confd_recv()
        wx.CallAfter(pub.sendMessage, "setInputStatus", status=1)
        # self.set_input_status(1)

    def get_list_data(self,socket):
        data_all = ""
        while True:
            data = socket.recv(8192)
            print(data.decode("utf-8"))
            data_all += data.decode("utf-8")
            if not data:
                break
        return data_all

    def clean_list(self):
        if self.list.NumberRows > 0:
            self.list.DeleteRows(0, self.list.NumberRows)

    def show_list(self,data_all):
        self.clean_list()
        list = self.list_reshape(data_all)
        for line in list:
            self.list.AppendRows()
            self.list.SetCellValue(self.list.NumberRows - 1, 0, line)
            self.list.SetReadOnly(self.list.NumberRows - 1, 0, True)

    def filename_from_path(self,command):
        name = ""
        for i in range(0, len(command)):
            if (command[len(command) - i - 1] != "/"):
                pass
            else:
                name = command[len(command) - (i):-2]
                break
        if name == "":
            name = command[5:-2]
        return name

    def cut_all_socket(self):
        if self.port_listen is not None:
            self.port_listen.close()
        if self.port_socket is not None:
            self.port_socket.close()
        if self.data_socket is not None:
            self.data_socket.close()
        self.s = None

    def get_port_ip_from_pasv(self,recv):
        group = re.search(r"(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)", recv, re.M)
        if group:
            self.datahost = group.group(1) + "." + group.group(2) + "." + group.group(3) + "." + group.group(4)
            self.dataport = int(group.group(5)) * 256 + int(group.group(6))

    def get_port_ip_from_port(self,info):
        ip = ""
        for i in range(0, 4):
            ip = ip + info[i]
            if  i < 3:
                ip = ip + '.'
        self.cport = int(info[4]) * 256 + int(info[5])
        self.port_listen = socket.socket()
        self.port_listen.bind((ip, self.cport))
        self.port_listen.listen(10)
        pass

    def Handler(self, command, recv):
        if len(command) < 4:
            return
        if command.upper()[0:4] == "PASV":
            if (recv[0:3] == "227"):
                self.set_code_color(True)
                self.get_port_ip_from_pasv(recv)
                self.data_mode = "PASV"
                return True
        elif command.upper()[0:4] == "PORT":
            if (recv[0:3] == "200"):
                self.set_code_color(True)

                info = command[:-2].split(" ")[1].split(',')
                self.get_port_ip_from_port(info)
                self.data_mode = "PORT"
                return True
        elif command.upper()[0:4] == "STOR":
            if recv[0:3] == "150":
                self.set_code_color(True)
                self.set_input_status(2)
                if self.data_mode == "PORT":
                    Thread(target=self.upload_p, args=(self.port_socket,command.split()[1])).start()
                    self.port_listen.close()
                    # self.upload(self.port_socket,command.split(" ")[1][0:-2])
                    # self.port_socket.close()
                    # self.port_listen.close()
                    # self.confd_recv()
                elif self.data_mode == "PASV":
                    Thread(target=self.upload_p, args=(self.data_socket,command.split()[1])).start()
                    # self.upload(self.data_socket,command.split(" ")[1][0:-2])
                    # self.data_socket.close()
                    # self.confd_recv()
                return True
        elif command.upper()[0:4] == "APPE":

            if recv[0:3] == "150":
                self.set_code_color(True)
                self.set_input_status(2)
                if self.data_mode == "PORT":
                    Thread(target=self.upload_p, args=(self.port_socket,command.split()[1])).start()
                    self.port_listen.close()
                    # self.upload(self.port_socket,command.split(" ")[1][0:-2])
                    # self.port_socket.close()
                    # self.port_listen.close()
                    # self.confd_recv()
                elif self.data_mode == "PASV":
                    Thread(target=self.upload_p, args=(self.data_socket,command.split()[1])).start()
                    # self.upload(self.data_socket,command.split(" ")[1][0:-2])
                    # self.data_socket.close()
                    # self.confd_recv()
                return True
        elif command[0:4].upper() == "RETR":
            self.set_input_status(2)
            if recv[0:3] == "150":
                self.set_code_color(True)
                size = None
                try:
                    size = re.search('\((\d+)',recv[3:],re.M).group()
                    size = int(size[1:])
                except:
                    pass

                thread = None
                if self.data_mode == "PASV":
                    name = self.filename_from_path(command)
                    thread = Thread(target=self.download_p, args=(self.data_socket,name,command.split()[1],size))
                    thread.start()
                elif self.data_mode == "PORT":
                    name = self.filename_from_path(command)
                    thread = Thread(target=self.download_p, args=(self.port_socket, name,command.split()[1],size))
                    thread.start()
                    self.port_listen.close()


                return True
        elif command[0:4].upper() == "LIST":
            if recv[0:3] == "150":
                self.set_code_color(True)
                if self.data_mode == "PASV":
                    data_all = self.get_list_data(self.data_socket)
                    self.data_socket.close()
                    self.confd_recv()
                    self.show_list(data_all)
                elif self.data_mode == "PORT":
                    data_all = self.get_list_data(self.port_socket)
                    self.port_socket.close()
                    self.port_listen.close()
                    self.confd_recv()
                    self.show_list(data_all)
                return True
        elif command[0:4].upper() == "QUIT" or command[0:4].upper() == "ABOR":
            if (recv[0:3] == "221"):
                self.set_code_color(True)
                self.cut_all_socket()
                self.Connect_Button.LabelText = "connect"
                self.set_input_status(0)
                return True
        elif command[0:4].upper() == "TYPE":
            if recv[0:3] == "200":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "PASS":
            if recv[0:3] == "230":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "USER":
            if recv[0:3] == "331":
                self.set_code_color(True)
                return True
        elif command[0:3].upper() == "PWD":
            if recv[0:3] == "257":
                self.set_code_color(True)
                return True
        elif command[0:3].upper() == "CWD":
            if recv[0:3] == "250":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "SYST":
            if recv[0:3] == "215":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "TYPE":
            if recv[0:3] == "200":
                self.set_code_color(True)
                return True
        elif command[0:3].upper() == "MKD":
            if recv[0:3] == "257":
                self.set_code_color(True)
                return True
        elif command[0:3].upper() == "RMD":
            if recv[0:3] == "250":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "REST":
            if recv[0:3] == "350":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "RNFR":
            if recv[0:3] == "350":
                self.set_code_color(True)
                return True
        elif command[0:4].upper() == "RNTO":
            if recv[0:3] == "250":
                self.set_code_color(True)
                return True
        self.set_code_color(False)
        return False
    def check_before_send(self,command):
        if command.upper()[0:4] == "STOR" or command.upper()[0:4] == "APPE":
            try:
                filepath = command.split()[1]
                fd = open(filepath,"rb")
            except:
                self.messageBox("no such file")
                return False
        return True

    def set_for_data(self,command):
        if command.upper()[0:4] == "STOR" or command.upper()[0:4] == "RETR" or command.upper()[0:4] == "LIST" or command.upper()[0:4] == "APPE":
            try:
                self.get_data_socket()
            except:
                pass
    def set_code_color(self,success):
        if success:
            self.history.SetCellBackgroundColour(0,1,wx.GREEN)
        else:
            self.history.SetCellTextColour(0,1,wx.WHITE)
            self.history.SetCellBackgroundColour(0,1,wx.RED)

    def con_msg_handler(self,command,show=True):
        if show:
            self.history.InsertRows(0, 1)
            self.history.SetCellValue(0, 0, command)
        if self.check_before_send(command):
            self.s.send(command.encode())
        else:
            self.set_input_status(1)
            return False
        self.set_for_data(command)
        data = self.confd_recv(show)
        return self.Handler(command=command,recv=data)

    def send_command(self,command):
        self.s.send(command.encode())
        pass

    def confd_recv(self,show=True):
        data = ""
        while True:
            char = self.s.recv(1).decode("utf-8")
            data += char
            if char == '\n':
                break
        # data = self.s.recv(8192).decode("utf-8")
        if show:
            wx.CallAfter(pub.sendMessage, "showConfdRecv", data=data)
            # self.history.SetReadOnly(0, 2, True)
        return data

    def send_button(self,event):
        # self.history.AppendRows(1)
        command = self.input_line.GetValue() + '\r' + '\n'
        if command != '\r\n':
            self.input_line.SetValue("")

            if not self.con_msg_handler(command):
                self.set_input_status(1)
        else:
            self.messageBox("please input command")

    def data_button(self,event):
        global download_switch,upload_switch,uncomplete_file,append_mode
        if event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to pause download":
            self.data_list.SetCellValue(event.Row,event.Col,"press to continue download")
            download_switch = 1
        elif event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to pause upload":
            self.data_list.SetCellValue(event.Row,event.Col,"press to continue upload")
            upload_switch = 1
        elif event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to continue download":
            self.data_list.SetCellValue(event.Row,event.Col,"press to pause download")
            if not self.continue_download(uncomplete_file[self.data_list.GetCellValue(event.Row,0)],self.data_list.GetCellValue(event.Row,0)):
                self.data_list.SetCellValue(event.Row, event.Col, "press to restart download")
        elif event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to restart download":
            uncomplete_file.pop(self.data_list.GetCellValue(event.Row,0))
            self.con_msg_handler("PASV\r\n")
            if self.con_msg_handler("RETR "+ self.data_list.GetCellValue(event.Row,0) + '\r\n'):
                pass
            else:
                append_mode = False
                self.data_list.DeleteRows(event.Row)
                self.messageBox("failed to restart download,please retry later")
                self.set_input_status(1)
        elif event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to restart upload":
            uncomplete_file.pop(self.data_list.GetCellValue(event.Row,0))
            self.con_msg_handler("PASV\r\n")
            if self.con_msg_handler("STOR "+ self.data_list.GetCellValue(event.Row,0) + '\r\n'):
                pass
            else:
                append_mode = False
                self.data_list.DeleteRows(event.Row)
                self.messageBox("failed to restart upload,please retry later")
                self.set_input_status(1)

        elif event.Col == 2 and self.data_list.GetCellValue(event.Row,event.Col) == "press to continue upload":
            self.data_list.SetCellValue(event.Row,event.Col,"press to pause upload")
            if not self.continue_upload(uncomplete_file[self.data_list.GetCellValue(event.Row,0)],self.data_list.GetCellValue(event.Row,0)):
                self.data_list.SetCellValue(event.Row, event.Col, "press to restart upload")

    def connect(self,event):
        if self.Connect_Button.LabelText == "connect":
            host = self.IP_Input.GetValue()
            if host == "":
                host = "127.0.0.1"
            port = self.Port_Input.GetValue()
            if port == "":
                port = "21"
            self.init_link(host,port)
            self.Connect_Button.LabelText = "disconnect"
        else:
            self.Connect_Button.LabelText = "connect"
            self.con_msg_handler("QUIT\r\n")


    def disconnect(self,event):
        print("disconnnect")
        print(self.s)
        if self.s != None:
            self.s.send("QUIT\r\n".encode("utf-8"))
            print(self.confd_recv())
        self.set_input_status(0)
        event.Skip()
        pass
    def messageBox(self,message):
        dlg = wx.MessageDialog(None,message, u"message",  wx.OK)
        if dlg.ShowModal() == wx.ID_OK:
            dlg.Destroy()

    def clearHistory(self):
        global uncomplete_file,append_mode,in_loading
        for i in range(0,self.history.NumberRows):
            self.history.DeleteRows()
        for i in range(0,self.data_list.NumberRows):
            self.data_list.DeleteRows()
        for i in range(0,self.list.NumberRows):
            self.list.DeleteRows()
        uncomplete_file = {}
        append_mode = False

        in_loading = False
    def indexInDataList(self,server_path):
        for i in range(0,self.data_list.NumberRows):
            if self.data_list.GetCellValue(i,0) == server_path:
                return i
        return self.data_list.NumberRows
    def showConfdRecv(self,data):
        if self.history.NumberRows > 0:
            if self.history.GetCellValue(0, 1) != "":
                self.history.InsertRows(0, 1, False)
        else:
            self.history.InsertRows(0, 1)
        self.history.SetCellValue(0, 2, data[4:])
        self.history.SetCellValue(0, 1, data[0:3])
        self.history.SetReadOnly(0, 0, True)
        self.history.SetReadOnly(0, 1, True)

class FtpApp(wx.App):
    def OnInit(self):
        frame = FTPFrame()
        frame.Show()
        return True


if __name__ == "__main__":
    app = FtpApp()
    app.MainLoop()