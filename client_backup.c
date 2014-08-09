#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<errno.h>
#include<unistd.h>
#include<stdbool.h>
#include<arpa/inet.h>
#include<pthread.h>
#include"include/list.h"
#include"common/constants.h"
#include"topology/topology.h"
#include"client/stcp_client.h"
#include<ncurses.h>

#define MAXLINE 8192
#define FRIEND_NUM 10
#define ONLINE_NUM 10
#define MESSAGE_NUM 5

#pragma pack(1)
typedef struct data_packet {

	char IM[4];
	char version;//01登录，02确认登录，03登录拒绝，04发给指定人，05发给所有人, 06退出IM, 07通知有人加入，08通知有人退出
	char username[8];//用户名
	char password[8];//密码
	char receiver[8];//接收者
	char message[120];//信息

}data_packet;//所用协议

static bool login_status = false, send_status = false, refuse = false,
			thread = false;//保证只生成一个进程
static char USERNAME[10];//存储当前登录的用户名

typedef struct OnlineUser {

	char username[10];
	int x;
	ListHead list;

}OnlineUser;

typedef struct Message {
	int hour, min, sec;
	char sender[10];
	char receiver[10];
	char data[120];
}Message;
Message Mes_send;
Message Mes_recv;
//存放接收到的消息

typedef struct Arg_Recv {
	int sockfd;
	char recvline[MAXLINE];
}Arg_Recv;

ListHead Online;
ListHead Send_mes;
ListHead Recv_mes;

extern int WritePacket();
extern int ResRecvPacket();
extern void Display_sta1();
extern void Display_sta2();
extern void Display_Friend();
extern void Display_help();
extern void Display_Message();
extern void *Recv();
extern void set_mes();

WINDOW *create_newwin(int height, int width, int starty, int startx) { 

	WINDOW *local_win; 
	local_win = newwin(height, width, starty, startx); 
//	box(local_win, 0 , 0); /* 0, 0 是字符默认的行列起始位置 */
	wrefresh(local_win); /*刷新窗口缓冲，显示 box */
	return local_win; 
} 

WINDOW *my_win;
WINDOW *my_win2;

//创建两个连接, 一个使用客户端端口号87和服务器端口号88. 另一个使用客户端端口号89和服务器端口号90.
#define CLIENTPORT1 87
#define SERVERPORT 88
//#define CLIENTPORT2 89
//#define SERVERPORT2 90

//在连接到SIP进程后, 等待1秒, 让服务器启动.
#define STARTDELAY 1

#define SERVERIP "127.0.0.1"
//这个函数连接到本地SIP进程的端口SIP_PORT. 如果TCP连接失败, 返回-1. 连接成功, 返回TCP套接字描述符, STCP将使用该描述符发送段.
int connectToSIP() {
	printf("===connectToSIP===\n");
	//你需要编写这里的代码.
	int sockfd;
	struct sockaddr_in servaddr;
	if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
		perror("client: Problem in creating sockfd!!\n");
		return -1;
	}
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(SERVERIP);
	servaddr.sin_port=htons(SIP_PORT);
	if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0){
		perror("client: problem in connecting to server!!\n");
		return -1;
	}
	return sockfd;

}

//这个函数断开到本地SIP进程的TCP连接. 
void disconnectToSIP(int sip_conn) {
	//你需要编写这里的代码.
	close(sip_conn);
}



int main() {
	srand(time(NULL));

	int sip_conn = connectToSIP();
	int CLIENTPORT = 0;
	if(sip_conn < 0) {
		printf("fail to connect to local SIP process\n");
		exit(1);
	}//连接到SIP进程

	//初始化客户端
	stcp_client_init(sip_conn);
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect: ");
	scanf("%s", hostname);
	int server_nodeID = topology_getNodeIDfromname(hostname);
	if(server_nodeID < 0) {
		printf("hostname error\n");
		exit(1);
	}
	printf("Connecting to node %d\n", server_nodeID);

	printf("Enter the PORT: ");
	scanf("%d", &CLIENTPORT);
	int sockfd = stcp_client_sock(CLIENTPORT);
	if(sockfd < 0) {
		printf("fail to create client stcp client sock\n");
		exit(1);
	}

	char recvline[MAXLINE];//接受数据时所用的字符数组，缓冲区
	data_packet send_data;
	Arg_Recv arg;//线程参数
	memset(&send_data, 0, sizeof(data_packet));//初始化数据包
	memset(&Mes_send, 0, sizeof(Message));//初始化Message
	memset(&Mes_recv, 0, sizeof(Message));

	if(stcp_client_connect(sockfd, server_nodeID, SERVERPORT) < 0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}//如何保证连接的服务器端口变化???

	//Create a socket for the client

	arg.sockfd = sockfd;
	list_init(&Online);

	initscr();
	cbreak();
	refresh();
	my_win2 = create_newwin(LINES, COLS/2, 0, COLS/2);
	my_win = create_newwin(LINES, COLS/2, 0, 0);
	scroll(my_win);
	scroll(my_win2);

	//Interaction with the serve
	while(1) {
		if(!login_status) {//登录
			wprintw(my_win, "before sockfd: %d\n", sockfd);
			if(WritePacket(&send_data) == 0)
				exit(0);//登录界面上退出
			wprintw(my_win, "after sockfd: %d\n", sockfd);
			if(stcp_client_send(sockfd, &send_data,sizeof(send_data))<0) {
				perror("Problem in write packet\n");
				exit(0);
			}
			if(stcp_client_recv(sockfd, recvline, MAXLINE) < 0) {
				perror("The server terminated prematurely");
				exit(0);
			}
			ResRecvPacket(recvline);
		}
		else {//已登录
			if(thread == false) {
				pthread_t i = (pthread_t)sockfd;
				int rc = pthread_create(&i, NULL, Recv, (void *)&arg);//创建线程接收消息
				if(rc) {
					wprintw(my_win, "Error:return code is %d\n", rc);
					exit(-1);
				}//先创建监听线程
				thread = true;
			}
			if(WritePacket(&send_data) == 0) {//写数据报
				wprintw(my_win, "Exit the client\n");
				break;
			}
			if(stcp_client_send(sockfd, &send_data, sizeof(send_data)) < 0) {
				wprintw(my_win, "Problem in write to the socket");
				exit(0);
			
		}
		memset(recvline, 0, MAXLINE);
		memset(&send_data, 0, sizeof(send_data));
		}
	}
	close(sockfd);
	endwin();
	return 0;
}

void Display_sta1() {
//	system("clear");
	wclear(my_win);
	wprintw(my_win, "Welcome to PY IM Client\n(l)login,(#)exit,(c)cln\n");
//	box(my_win, 0, 0);
	wrefresh(my_win);
}

void Display_sta2() {
//	system("clear");
	wclear(my_win);
	wprintw(my_win, "%s Welcome to PY IMnet\n(r)quit, (c)cln, (h)help\n(#)exit, (s)send, (a)sendtoall\n====================================\n", USERNAME);
	Display_Friend();
//	Display_Message();	
//	box(my_win, 0, 0);
	wrefresh(my_win);
}
void Display_help() {
//	system("clear");
//	clear();
	wprintw(my_win, "Help info\n");
	wprintw(my_win, "Press 'r' to return\n");
}


void Display_Message() {
	//printw("\n===============Message==============\n");
	wprintw(my_win2, "Send:%s-->%s:%s\t%d:%d:%d\n",
			Mes_send.sender, Mes_send.receiver, Mes_send.data,
			Mes_send.hour, Mes_send.min, Mes_send.sec);
	wprintw(my_win2, "Recv:%s-->%s:%s\t%d:%d:%d\n", 
			Mes_recv.sender, Mes_recv.receiver, Mes_recv.data,
			Mes_recv.hour, Mes_recv.min, Mes_recv.sec);
	//printw("\n====================================\n");
	//	box(my_win2, 0, 0);
	wrefresh(my_win2);
}

void Display_Friend() {
	//显示好友列表
	//////////////
	//显示在线人员列表
	wprintw(my_win, "\n============Online member===========\n");
	ListHead *current;
	OnlineUser *ptr = NULL;
	list_foreach(current, &Online) {
		ptr = list_entry(current, OnlineUser, list);
		wprintw(my_win, "%s\n", ptr->username);
	}
	wprintw(my_win, "\n====================================\n");
}

void set_mes(Message *Mes, char *sender, char *receiver, char *message) {
	time_t t = time(NULL);
	struct tm* local;
	local = localtime(&t);
	Mes->hour = local->tm_hour;
	Mes->min = local->tm_min;
	Mes->sec = local->tm_sec;
	strncpy(Mes->sender, sender, 8);
	strncpy(Mes->receiver, receiver, 8);
	strncpy(Mes->data, message, 120);
}//设置信息
		

int WritePacket(data_packet *packet) {
	strncpy((*packet).IM, "PY", 2);
	
	if(!login_status) {//未登录状态
		Display_sta1();
		if(refuse) {//被拒绝
			wprintw(my_win, "Sorry, username or password worng\n or you has logined\n");
			wrefresh(my_win);
			refuse = false;
		}
		while(1) {
			char Login[1];
			scanw("%s", Login);
			if(strcmp(Login, "l") == 0) {
				char username[10], password[10];
				memset(password, 0, 10);
				memset(username, 0, 10);
				wprintw(my_win, "Please enter the username: ");
				wscanw(my_win, "%s", username);//输入数据
				wprintw(my_win, "Please enter the password: ");
				noecho();
				wscanw(my_win, "%s", password);
				echo();
				(*packet).version = 0x01;
				strncpy((*packet).username, username, 8);
				strncpy((*packet).password, password, 8);
				break;
			}
			else if(strcmp(Login, "#") == 0){ return 0;}//退出客户
			else if(strcmp(Login, "c") == 0){Display_sta1();}//清屏
			else {wprintw(my_win, "input error\n");}
		}
	}
	else {//已登录状态
		strncpy((*packet).username, USERNAME, 8);
		Display_sta2();
		char handle[1];
		while(1) {
			wscanw(my_win, "%s", handle);//操作客户端
			switch(handle[0]) {
				case 'r':(*packet).version = 0x06;
						 login_status = false;
						 wclear(my_win2);
						 //必须在这里修改，否则会有显示错误
						 return 1;//注销
				case 'h':Display_help();
						 char back[1];
						 while(1) {
							 wscanw(my_win, "%s", back);
							 if(strcmp(back, "r") == 0) {
								 Display_sta2();
								 break;
							 }	
						}
						break;//help

				case 's':{
						 send_status = true;//进入发送状态
						 (*packet).version = 0x04;
						 char receiver[10], message[120];// c;
						 memset(receiver, 0, 10);
						 memset(message, 0, 120);
						 wprintw(my_win, "Enter the receiver: ");
						 wscanw(my_win, "%s", receiver);
						//	 c = getchar();//吸收掉一个野\n
						 wprintw(my_win, "Enter the message: ");
						 wscanw(my_win, "%[^\n]", message);
						 strncpy((*packet).receiver, receiver, 8);
						 strncpy((*packet).message, message, 120);
						 set_mes(&Mes_send,USERNAME,receiver, message);
						 send_status = false;
						 Display_Message();
						 return 1;//发送给特定人
						}

				case 'a':{
						 send_status = true;//进入发送信息的状态
						 (*packet).version = 0x05;
						 wprintw(my_win, "Enter the message:");
						 char message[120];// c;
						// c = getchar();
						 wscanw(my_win, "%[^\n]", message);
						 strncpy((*packet).message, message, 120);
						 set_mes(&Mes_send, USERNAME, "Everyone", message);
						 send_status = false;
						 Display_Message();
						 return 1;//发送给所有人
						}

				case 'c':Display_sta2();
						 break;//刷屏

				case '#':return 0;//退出客户端
				default:wprintw(my_win, "input error\n");
			}
		}
	}
	return 1;

}//自定义输入函数


int ResRecvPacket(char* recvbuf) {
	if(strncmp("PY", recvbuf, 2) == 0) {
		switch(recvbuf[4]) {
			case 0x02:login_status = true;//进入状态2
					  strncpy(USERNAME, &recvbuf[5], 8);
					  int i = 133;
					  for(; recvbuf[i] != '\0'; i += 10) {
						  OnlineUser *user = (OnlineUser*)malloc(sizeof(OnlineUser));
						  strncpy(user->username, &(recvbuf[i]), 8);
						  list_add_after(&Online, &(user->list));
					  }
					  break;//确认登录

			case 0x03:refuse = true;
					  break;//拒绝登录

			case 0x04:if(login_status) {
						  char sender[10], message[120];
						  strncpy(sender, &recvbuf[5], 8);
						  strncpy(message, &recvbuf[13], 120);
						  set_mes(&Mes_recv, sender, USERNAME,message);
						  if(!send_status)
							  Display_Message();
						  // Display_sta2();
						  }
					  break;//收到信息
			case 0x05:if(login_status) {
						  char sender[10], message[120];
						  strncpy(sender, &recvbuf[5], 8);
						  strncpy(message, &recvbuf[13], 120);
						  set_mes(&Mes_recv, sender, "Everyone", message);
						  if(!send_status)
							  Display_Message();
							  //Display_sta2();
					  }
					  break;

			case 0x07:{if(login_status) {//登录状态下才解析
						  OnlineUser *user = (OnlineUser *)malloc(sizeof(OnlineUser));//在堆中分配空间
						  strncpy(user->username, &recvbuf[5], 8);
							  
						  ListHead *current = NULL;
						  OnlineUser *ptr = NULL;
						  list_foreach(current, &Online) {
							  ptr = list_entry(current, OnlineUser, list);
							  if(strncmp(ptr->username, user->username, 8) == 0)
								  break;
						  }
						  if(current == &Online)//这个用户没有在链表中
							  list_add_after(&Online,&(user->list));//加入在线队列
						  else//用户已经在链表中
							  free(user);
						  if(!send_status) 
							  Display_sta2(); //有人上线
					  }
					  break;
					  }

			case 0x08: {//登录状态下才解析
					  char member_quit[10];
					  ListHead *current;
					  OnlineUser *ptr = NULL;
					  strncpy(member_quit, &recvbuf[5], 8);
					  if(strncmp(USERNAME, member_quit, 8) == 0) {
						  list_foreach(current, &Online) {
							  ptr = list_entry(current, OnlineUser, list);
							  list_del(current);
							  free(ptr);
						  }
						  thread = false;
						  pthread_exit(NULL);
						  return 0;//注销(终结线程)
					  }
					  else if(login_status) {
						  list_foreach(current, &Online) {
							  ptr = list_entry(current, OnlineUser, list);
							  if(strncmp(ptr->username, member_quit,8)==0) {
								  list_del(current);//移出在线队列
								  free(ptr);
							  }
						  } 
						  if(!send_status)
							  Display_sta2();
							  //有人下线
					  }
					  break;
					 }
			default:wprintw(my_win, "Message received cannot be resuloted by %d\n", recvbuf[4]);
						  break;
		}
	}
	return 1;
}

void *Recv(Arg_Recv *arg) {
		int sockfd = arg->sockfd;
		char recvline[MAXLINE];
		strncpy(recvline, arg->recvline, 120);
		int n = 0;
		while((n = stcp_client_recv(sockfd, recvline, MAXLINE)) > 0) {
		//	wprintw(my_win, "Pthread receive something\n");
			if(ResRecvPacket(recvline) == 0) {
		//		wprintw(my_win, "Pthread over\n");
			return 0;
		}
		memset(recvline, 0, sizeof(recvline));
	}
	return 0;
}
