//本文件是建立在简单重叠网络之上的IM系统的客服务器端部分
//IM源代码来自浦阳同学
//created on 2014-5-10
//created by mengzs

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "common/constants.h"
#include "server/stcp_server.h"
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "include/list.h"
#include <asm/ioctls.h>
#include <sys/ioctl.h>


//IM程序中一些宏定义
#define MAXLINE 4096
//#define LISTENQ 8
#define NUM_THREADS 8
#define NUM_USER 5

#define CLIENTPORT 87
#define SERVERPORT 88
//#define CLIENTPORT2 89
//#define SERVERPORT2 90
#define WAITTIME 15
#define SERVERIP "127.0.0.1"



//协议数据报
#pragma pack(1)
typedef struct data_packet {

	char IM[4];//标志为本网络的数据报
	char version;//01登录，02发给特定人, 03发给所有人
	char sender[8];//发送者用户名
	char message[120];
	char online[80];

}data_packet;

//线程函数的参数
typedef struct Arg {

	int n;
	char recvbuf[MAXLINE];//接收到的数据
	int connfd;
	data_packet data;//发出去的数据协议

}Arg;

typedef struct User {
	
	char username[8];
	int connfd;//与该用户连接的套接字	
	ListHead list;

}User;

ListHead runq;

//预存储已注册的用户
char *USER[NUM_USER] = {"py", "qxq", "nxf", "slf", "zx"};
char *PWD[NUM_USER] = {"27543602", "q8000", "dfy", "sfq", "0529"};

extern void *ResWritePacket();
extern bool User_exist();
extern int connectToSIP(); //返回TCP套接字描述符
extern void disconnectToSIP(int sip_conn);//断开连接

int connectToSIP() {
	printf("===connectToSIP===\n");
	int sockfd;
	struct sockaddr_in servaddr;
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		perror("server: Problem in creating sockfd!!\n");
		return -1;
	}
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(SERVERIP);
	servaddr.sin_port=htons(SIP_PORT);
	if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0){
		perror("server: problem in connecting to server!!\n");
		return -1;
	}
	return sockfd;	
}

//断开连接
void disconnectToSIP(int sip_conn) {
	close(sip_conn);
}

//判断用户是否应注册
bool User_exist(char *user, char *pwd) {
	int i = 0;
	for(; i < NUM_USER; i++) {
		if(strncmp(user, USER[i], 8) == 0) {
			if(strncmp(pwd, PWD[i], 8) == 0) {
				printf("%s is existed\n", user);
				return true;
			}
			else
				return false;
		}
	}
	return false;
}


void *ResWritePacket(void *arg) {
	int connfd = ((Arg *)arg)->connfd;
	char recvbuf[MAXLINE];
	strncpy(recvbuf, ((Arg *)arg)->recvbuf, MAXLINE);
	data_packet data;
	int n = ((Arg *)arg)->n;

	printf("%d pthread running\n", connfd);

	while((n = stcp_server_recv(connfd, recvbuf, MAXLINE)) > 0){
		
		if(strncmp("PY", &(recvbuf[0]), 2) == 0) {//解析包头
			switch(recvbuf[4]) {
				case 0x01:{char user[10], pwd[10];//解析用户名，密码
						   bool exist = false;
						  strncpy(user, &(recvbuf[5]), 8);
						  strncpy(pwd, &(recvbuf[13]), 8);
						  printf("Username:%s\n", user);
						  printf("PWD:%s\n", pwd);
						  if(User_exist(user, pwd)) {//该用户已注册
							  strncpy(data.IM, "PY", 2);
							  ListHead *current = NULL;
							  User *ptr;
							  int i = 0;
							  list_foreach(current, &runq) {
								  ptr =list_entry(current, User, list);
								  if(strncmp(user,ptr->username,8)==0)
									  exist = true;
								  else {
									  strncpy(&(data.online[i]), 
											  ptr->username, 8);
									  i += 10;
								  }
							  }
							  if(exist == true) {
								  data.version = 0x03;
								  strncpy(data.sender, user, 8);
								  stcp_server_send(connfd, &data, sizeof(data));
								  printf("Refuse:%s has login\n",data.sender);
								  //该用户已在别处登录
							  }
							  else {
								  data.version = 0x02;
								  strncpy(data.sender, user, 8); 
								  stcp_server_send(connfd, &data, sizeof(data));//发给客户端确认其登录
								  printf("%s login by %d\n", data.sender, connfd);

								  User *new_user =(User *)malloc(sizeof(User));
								  list_init(&(new_user->list));
								  strncpy(new_user->username, user, 8);
								  new_user->connfd = connfd;
								  list_add_after(&runq, &(new_user->list));//将登录的用户加入队列
								  data.version = 0x07;
								  list_foreach(current, &runq) {
									  ptr =list_entry(current, User, list);
									  if(strncmp(user, ptr->username, 8) != 0) {
										  stcp_server_send(ptr->connfd, &data, sizeof(data));
										  printf("Send to %s to Confirm the login\n", ptr->username);
									  }
								  }//广播有人上线
							  }
						  }
						  else {
							  strncpy(data.IM, "PY", 2);
							  data.version = 0x03;//拒绝登录
							  strncpy(data.sender, user, 8);
							  stcp_server_send(connfd, &data, sizeof(data));
						  }
						  break;//回复是否可登录
						  }

				case 0x04:{printf("Receive the send_to_man packet\n");
						  char receiver[10];
						  data.version = 0x04;
					      strncpy(data.IM, "PY", 2);
						  strncpy(data.sender, &(recvbuf[5]), 8);
						  strncpy(data.message, &(recvbuf[29]), 120);
						  strncpy(receiver, &(recvbuf[21]), 8);
						  ListHead *current;
						  User *ptr = NULL;
						  list_foreach(current, &runq) {
							  ptr = list_entry(current, User, list);
							  if(strncmp(ptr->username, receiver, 8) == 0) {
								  printf("target:%s\n", ptr->username);
								  stcp_server_send(ptr->connfd, &data, sizeof(data));
								  printf("%s->%s connfd:%d\n", (data).sender, receiver, ptr->connfd);
							  }
						  }
						  break;//发给指定人
						  }

				case 0x05:{printf("Receive the send_to_all packet\n");
						  strncpy(data.IM, "PY", 2);
						  data.version = 0x05;
						  strncpy(data.sender, &(recvbuf[5]), 8);
						  strncpy(data.message, &(recvbuf[29]), 120);
						  ListHead *current;
						  User *ptr = NULL;
						  list_foreach(current, &runq) {
							  ptr = list_entry(current, User, list);
							  if(strncmp(ptr->username, data.sender, 8) != 0)
								  stcp_server_send(ptr->connfd, &data,sizeof(data));
							  }
						  }
						  break;//发送消息给所有人
				case 0x06:{
							  strncpy(data.IM, "PY", 2);
							  data.version = 0x08;
							  printf("version:%d\n", data.version);
							  strncpy(data.sender, &(recvbuf[5]), 8);
							  User *ptr = NULL;
							  ListHead *current;
							  list_foreach(current, &runq) {
								  ptr = list_entry(current, User,list);
								  stcp_server_send(ptr->connfd, &data, sizeof(data));//通知其它用户有人退出
								  printf("Send %s exit\n",data.sender);
								  if(strncmp(ptr->username, &(recvbuf[5]), 8) == 0) {//在链表中找到退出的用户
									  printf("%s has been del\n",data.sender);
									  list_del(current);//删除用户
									  free(ptr);
								  }
							  }
						  //有人退出
						  break;
						  }
				default:printf("The packet can't be resulote\n");
						break;
			}
		}
		else
			printf("The packet is not belong to this IM\n");

		memset(&data, 0, sizeof(data));
		memset(recvbuf, 0, MAXLINE);
	}
	close(connfd);

	//客户端异常关闭的情况
	printf("The exit connfd：%d\n", connfd);
	char member_quit[10];
	User *ptr = NULL;
	ListHead *current;
	list_foreach(current, &runq) {
		ptr = list_entry(current, User, list);
		if(ptr->connfd == connfd) {//通过套接字找到退出的用户
			strncpy(member_quit, ptr->username, 8);
			list_del(current);//删除用户
			printf("%s has been deleted\n", ptr->username);
			free(ptr);
			break;
		}
	}
	strncpy(data.IM, "PY", 2);
	data.version = 0x08;
	strncpy(data.sender, member_quit, 8);
	list_foreach(current, &runq) {
		ptr = list_entry(current, User, list);
		stcp_server_send(ptr->connfd, &data, sizeof(data));
		printf("Inform %s that %s exit\n", ptr->username, data.sender);
	}//通知其它用户
	pthread_exit(NULL);
	return 0;
}

int main() {
	//丢包率的种子
	srand(time(NULL));

	//连接到SIP，TCP套接字
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
		exit(1);
	}
	stcp_server_init(sip_conn);
	list_init(&runq);
	//无限循环中不停接收客户端的连接请求
	int port_index=0;
	while(1) {
		//每次新来
		int sockfd;
		Arg arg;
		if((sockfd = stcp_server_sock(SERVERPORT + port_index))<0){
			printf("can't create stcp server\n");
			exit(1);
		}
		stcp_server_accept(sockfd);//该函数为阻塞函数
		printf("Received request\n");

		arg.connfd = sockfd;
		pthread_t i = (pthread_t)arg.connfd;
		printf("Create listening thread\n");
		int rc = pthread_create(&i, NULL, ResWritePacket,(void *)&arg);
		if(rc) {
			perror("Problem in creating pthread\n");
		}//每个套接字创建线程
		port_index += 2;  //多客户端一个连接请求则服务器端口号加2
	}
	return 0;
}

