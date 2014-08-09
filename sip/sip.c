//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2013年1月

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 10

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
	//你需要编写这里的代码.
    int sockfd;
	struct sockaddr_in servaddr;
	if((sockfd=socket(AF_INET,SOCK_STREAM,0))<0){
		printf("SIP: problem in creating socket\n");
		return -1;
	}
	memset(&servaddr,0,sizeof(struct sockaddr_in));
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=inet_addr("127.0.0.1");
	servaddr.sin_port=htons(SON_PORT);
    if(connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0){
        printf("SIP: problem in connecting server\n");
        return -1;
    }
    return sockfd;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	//你需要编写这里的代码.
    /*char buf[20];
    memset(buf,0,20);
    ((sip_pkt_t*)buf)->header.src_nodeID=topology_getMyNodeID();
    ((sip_pkt_t*)buf)->header.dest_nodeID=BROADCAST_NODEID;
    ((sip_pkt_t*)buf)->header.length=4;
    ((sip_pkt_t*)buf)->header.type=ROUTE_UPDATE;
    ((pkt_routeupdate_t*)(buf+12))->entryNum=0;
    while(1){
        sleep(ROUTEUPDATE_INTERVAL);
        son_sendpkt(BROADCAST_NODEID,(sip_pkt_t*)buf,son_conn);
    }*/
	int node_num=topology_getNodeNum();
	int nbr_num=topology_getNbrNum();
	sip_pkt_t *pkt=(sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	memset(pkt,0,sizeof(sip_pkt_t));
	pkt->header.src_nodeID=topology_getMyNodeID();
	pkt->header.dest_nodeID=BROADCAST_NODEID;
	pkt->header.length=sizeof(dv_entry_t) *node_num+4;
	pkt->header.type=ROUTE_UPDATE;
	((pkt_routeupdate_t*)pkt->data)->entryNum=node_num;
	int i,offset=0;
	while(1){
		for(i=0;i<node_num;i++){
			memcpy(pkt->data+4+offset,&dv[nbr_num].dvEntry[i].nodeID,4);
			memcpy(pkt->data+8+offset,&dv[nbr_num].dvEntry[i].cost,4);
			offset+=8;
		}
		son_sendpkt(BROADCAST_NODEID,pkt,son_conn);
		sleep(ROUTEUPDATE_INTERVAL);
		offset=0;
	}
	pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	//你需要编写这里的代码.
  	/*sip_pkt_t pkt;

	while(son_recvpkt(&pkt,son_conn)>0) {
		printf("Routing: received a packet from neighbor %d\n",pkt.header.src_nodeID);
	}
	close(son_conn);
	son_conn = -1;
	pthread_exit(NULL);*/
	sip_pkt_t *pkt;
	pkt=(sip_pkt_t *)malloc(sizeof(sip_pkt_t));
	int my_id=topology_getMyNodeID();
	int nbr_num=topology_getNbrNum();
	int i=0;
	while(son_recvpkt(pkt,son_conn)>0){
		printf("Routing: received a packet from neighbor %d\n",pkt->header.src_nodeID);
		if(pkt->header.type==SIP){
			printf("----secv a sip----\n");
			if(pkt->header.dest_nodeID==my_id)
				forwardsegToSTCP(stcp_conn,pkt->header.src_nodeID,(seg_t*)pkt->data);
			else{
				int nextNodeID=routingtable_getnextnode(routingtable,pkt->header.dest_nodeID);
				if(nextNodeID!=-1) 
                    son_sendpkt(nextNodeID,pkt,son_conn);
			}
		}
		else if(pkt->header.type==ROUTE_UPDATE){
			printf("sip routeupdate!!!\n");
			int index;
			for(i=0;i<nbr_num;i++){
				if(dv[i].nodeID==pkt->header.src_nodeID){
					index=i;
					break;
				}
			}
			int count=((pkt_routeupdate_t *)pkt->data)->entryNum;
			int offset=4;
			for(i=0;i<count;i++){
				dv[index].dvEntry[i].nodeID=((routeupdate_entry_t*)(pkt->data+offset))->nodeID;
				dv[index].dvEntry[i].cost=((routeupdate_entry_t*)(pkt->data+offset))->cost;
				offset+=8;
			}
			int distance;
			for(i=0;i<count;i++)
				if(dv[index].dvEntry[i].nodeID==dv[index].nodeID)
					distance=dv[nbr_num].dvEntry[i].cost;

			for(i=0;i<count;i++){
				if(dv[nbr_num].dvEntry[i].cost>dv[index].dvEntry[i].cost+distance){
					dv[nbr_num].dvEntry[i].cost=dv[index].dvEntry[i].cost+distance;
					routingtable_setnextnode(routingtable,dv[nbr_num].dvEntry[i].nodeID,dv[index].nodeID);
					
				}
			}
		}
		else
			printf("error pkt type !!!\n");
	}
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	//你需要编写这里的代码.
    close(son_conn);
	close(stcp_conn);
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
	//你需要编写这里的代码.
	int listenfd;
	struct sockaddr_in servaddr;
	if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0){
		printf("sockfd creanted error!!!\n");
		exit(-1);
	}
	servaddr.sin_family=AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(SIP_PORT);
	if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0) {
		printf("bind sockfd error!!\n");
		exit(-1);
	}
	if(listen(listenfd, 1) < 0) {
		printf("listen sockfd error!!\n");
		exit(-1);
	}
    stcp_conn=-1;
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	while(1){
        if(stcp_conn==-1&&(stcp_conn = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen))<0) {
	    	printf("accept error!!\n");
	    	exit(-1);
	    }
	    seg_t* seg = (seg_t*)malloc(sizeof(seg_t));
	    char buffer[sizeof(seg_t) + sizeof(sip_hdr_t)];
	    int dest_NodeID;
	    int nextNodeID;
		if(getsegToSend(stcp_conn, &dest_NodeID, seg)==-1){
		    sleep(2);
            stcp_conn=-1;
            continue;
        }
		((sip_pkt_t*)buffer)->header.src_nodeID = topology_getMyNodeID();
		((sip_pkt_t*)buffer)->header.dest_nodeID = dest_NodeID;
		((sip_pkt_t*)buffer)->header.length = sizeof(stcp_hdr_t) + seg->header.length;
		((sip_pkt_t*)buffer)->header.type = SIP;
		memcpy(buffer + sizeof(sip_hdr_t), seg, sizeof(seg_t));
		nextNodeID = routingtable_getnextnode(routingtable, dest_NodeID);
		if(nextNodeID!=-1)
            son_sendpkt(nextNodeID, (sip_pkt_t*)buffer, son_conn);
	}
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}


