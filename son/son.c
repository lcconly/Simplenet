//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2013年


#include "son.h"


//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 10

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 


/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {

	//你需要编写这里的代码.
	printf("waitNbrs\n");
	int listenfd,connfd;
	struct sockaddr_in cliaddr,servaddr; 
	if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0){  //监听套解字建立失败
		perror("Problem in creating the socket");
		exit(0);
	}
	int hostID=topology_getMyNodeID();  //本机ID
	int num=topology_getNbrNum();   //邻居数
	int con_nbr_num=0;  //节点ID比自己大的结点数目
	int* nbr_ID=topology_getNbrArray();  //所有邻居结点的ID
	int i=0;
	for(;i<num;i++){
		if(nbr_ID[i]>hostID)
			con_nbr_num++;  //用来统计ID比本机大的结点数目
	}
	//准备套接字地址
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(CONNECTION_PORT);

	//绑定套接字
	bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	listen(listenfd,MAX_NODE_NUM);
	socklen_t clilen=sizeof(cliaddr);
	i=0;
	for(;i<con_nbr_num;){
		memset(&cliaddr,0,sizeof(cliaddr));
		connfd=accept(listenfd,(struct sockaddr*)&cliaddr,&clilen);    
			//accept函数是阻塞函数
		int nodeID=topology_getNodeIDfromip((struct in_addr*)&cliaddr.sin_addr);  //这里的类型要转换
		printf("nodeID %d \n",nodeID);
		if(nodeID<hostID||nodeID==0){  // 127.0.0.1
			close(connfd);  //不合条件的链接不会被准许
		}
		else
		{
			//为邻接表分配TCP链接
			nt_addconn(nt,nodeID,connfd);
			i++;
		}
	
	}
	close(listenfd);
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {

	//你需要编写这里的代码.
	printf("connectNbrs\n");
	int sockfd;
	struct sockaddr_in servaddr;
	int	 hostID=topology_getMyNodeID();  //本机ID
	int num=topology_getNbrNum();  //邻居数
	//int con_nbr_num=0;  //比本机节点ID小的结点ID数目
	int* nbr_ID=topology_getNbrArray();
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(CONNECTION_PORT);
	int j=0;
	while(j<num){
		if(nbr_ID[j]<hostID){ //如果ID小于本机ID,则试图去链接
			printf("connetctNbrs 有小于自己的邻居，试图去链接\n");
			if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
				perror("Problem in creating the socket\n");
				return -1;
			}
			printf("connct 添加套接字deID  %d\n",nbr_ID[j]);
			servaddr.sin_addr.s_addr=nt_getIP(nt,num,nbr_ID[j]);   //网络字节序
			printf("%s\n",inet_ntoa(servaddr.sin_addr));
			
			if(connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
				perror("Problem in connecting to the server\n");
				return -1;
			}
			printf("connct 添加套接字\n");
			if(nt_addconn(nt,nbr_ID[j],sockfd)==1){
				printf("connet 添加套接字成功\n");
			}

		}
		j++;
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的.
void* listen_to_neighbor(void* arg) {

	printf("listentonbr  jinru\n");
	int index=*(int *)arg;    //邻居表索引
	//int nodeID=nt[index].nodeID;  //节点ID
	int conn=nt[index].conn;  //对应邻居节点套接字
	sip_pkt_t* temp=(sip_pkt_t*)malloc(sizeof(sip_pkt_t));
	memset(temp,0,sizeof(sip_pkt_t)); //初始化
	while(recvpkt((sip_pkt_t*)temp,conn)>0){  //接受邻居结点成功,持续接受
		if(sip_conn==-1){
            printf("sip has been closed!!\n");
            continue;
            //printf("sip has been closed!!\n");
        }
        if(forwardpktToSIP(temp,sip_conn)>0){
			printf("发给本地sip成功\n");
		}
	}
    nt[index].conn=-1;
	printf("listen_to_neib exit\n");
	//你需要编写这里的代码.
	pthread_exit(NULL);
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP(){
	printf("wait SIP进入\n");
	int listenfd,connfd;
	struct sockaddr_in cliaddr,servaddr;
	if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0){
		perror("Problem in creating the socket\n");
		exit(0);
	}
	//准备套接字地址
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(SON_PORT);  //打开端口等待SIP进程的进入连接
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	listen(listenfd,MAX_NODE_NUM);
	socklen_t clilen=sizeof(cliaddr);
	while(1){
		printf("while accept!!\n");
		if((connfd=accept(listenfd,(struct sockaddr*)&cliaddr,&clilen))>0){
		//连接成功
			printf("waitSIP进入并链接成功\n");
			sip_conn=connfd;    //本地TCP连接套接字
			sendpkt_arg_t pkt;
			memset((sendpkt_arg_t*)&pkt,0,sizeof(sendpkt_arg_t));
			while(getpktToSend(&(pkt.pkt),&(pkt.nextNodeID),sip_conn)>0){
				printf("收到了sip\n");
				if(pkt.nextNodeID==BROADCAST_NODEID){  //发给所有的邻居结点
					printf("收到了sip路由更新报文\n");
					int nbr_num=topology_getNbrNum();  //邻居结点数目
					int i=0;
					for(;i<nbr_num;i++){
						sendpkt(&(pkt.pkt),nt[i].conn);
					}
				}
				else//发给下一个结点
				{
					//根据ID找到conn
					int nbr_num=topology_getNbrNum();
					int i=0;
					int next=0;
					for(;i<nbr_num;i++){
						if(nt[i].nodeID==pkt.nextNodeID){
							next=nt[i].conn;
							break;
						}
					}
					if(i==nbr_num){
						printf("吓一跳套接字找补到\n");
						next=-1;
						continue;
					}
					sendpkt(&(pkt.pkt),next);  //发给下一跳
				}
			}
		}
	    sip_conn=-1;
        sleep(2);
	}
	pthread_exit(NULL);
}


//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {

	close(sip_conn);
	nt_destroy(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	
	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	printf("nbrNUM:    %d\n",nbrNum);
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}
	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");
	
	//等待来自SIP进程的连接
	waitSIP();
}
