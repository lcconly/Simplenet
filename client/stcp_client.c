//文件名: client/stcp_client.c
//
//描述: 这个文件包含STCP客户端接口实现 
//
//创建日期: 2013年1月
#define _BSD_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "../topology/topology.h"
#include "stcp_client.h"
#include "../common/seg.h"

//声明tcbtable为全局变量
client_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//声明到SIP进程的TCP连接为全局变量
int sip_conn;

/*********************************************************************/
//
//STCP API实现
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
void stcp_client_init(int conn) 
{
	printf("===stcp_client_init===\n");
	int i=0;
	for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
		tcbtable[i]=NULL;                /*初始化TCB表为NULL*/
	sip_conn=conn;
	/*启动seghandler线程*/
	pthread_t tid;
	if(pthread_create(&tid, NULL, seghandler, NULL) == -1)
		perror("Start the thread unsuccessfully!\n");
	return;

}

// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
int stcp_client_sock(unsigned int client_port) 
{

	printf("===stcp_client_sock===\n");
	int i=0;
	for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
		if(tcbtable[i]==NULL)
			break;                          /*找到第一个NULL条目*/
	if(i==MAX_TRANSPORT_CONNECTIONS)
		return -1;						/*TCB表中无条目可用，则返回-1*/
	tcbtable[i]=(client_tcb_t*)malloc(sizeof(client_tcb_t));
	memset(tcbtable[i],0,sizeof(client_tcb_t));
	/*初始化数据域*/
	tcbtable[i]->state=CLOSED;
	tcbtable[i]->client_portNum=client_port;
	tcbtable[i]->client_nodeID=topology_getMyNodeID();
    tcbtable[i]->next_seqNum=0;
    tcbtable[i]->bufMutex=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(tcbtable[i]->bufMutex,NULL);
    tcbtable[i]->sendBufHead=NULL;
    tcbtable[i]->sendBufTail=NULL;
    tcbtable[i]->sendBufunSent=NULL;
    tcbtable[i]->unAck_segNum=0;
	//客户端数据接收缓冲区
	tcbtable[i]->recvBuf = (char* )malloc(RECEIVE_BUF_SIZE);
	tcbtable[i]->usedBufLen = 0;
	tcbtable[i]->bufMutex_recv = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(tcbtable[i]->bufMutex_recv, NULL);
	return i;

}

// 这个函数用于连接服务器. 它以套接字ID, 服务器节点ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器节点ID和服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("===stcp_client_connect===\n");
	/*sockfd的取值范围会在调用此函数时先行检查，所以这里就不再确认*/
	tcbtable[sockfd]->server_portNum=server_port;
	tcbtable[sockfd]->server_nodeID=nodeID;
	seg_t* sendseg=(seg_t*)malloc(sizeof(seg_t));
	memset(sendseg,0,sizeof(seg_t));
	int i=0;
	clock_t start;  /**/
	for(;i<=SYN_MAX_RETRY;i++){    /*第一次发送和之后最大重传次数*/
		sendseg->header.src_port=tcbtable[sockfd]->client_portNum;
		sendseg->header.dest_port=server_port;
		sendseg->header.length=0;
		sendseg->header.type = SYN;
		sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,sendseg);  /*发送SYN给服务器*/
		tcbtable[sockfd]->state=SYNSENT;
		printf("client send SYN!!\n");
		start =clock();
		while(1){
			if(tcbtable[sockfd]->state == CONNECTED){  /*收到的是SYNACK数据包*/
				printf("Sockfd: %d Connect Successfully\n", sockfd);
				free(sendseg);
				return 1;
			}
			if(clock()-start>=SYN_TIMEOUT/1000)
				break;
		}									
	}
	if(i>SYN_MAX_RETRY){
		tcbtable[sockfd]->state=CLOSED;  
		return -1;   /*超过最大重传次数*/
	}
}

// 发送数据给STCP服务器. 这个函数使用套接字ID找到TCB表中的条目.
// 然后它使用提供的数据创建segBuf, 将它附加到发送缓冲区链表中.
// 如果发送缓冲区在插入数据之前为空, 一个名为sendbuf_timer的线程就会启动.
// 每隔SENDBUF_ROLLING_INTERVAL时间查询发送缓冲区以检查是否有超时事件发生. 
// 这个函数在成功时返回1，否则返回-1. 
// stcp_client_send是一个非阻塞函数调用.
// 因为用户数据被分片为固定大小的STCP段, 所以一次stcp_client_send调用可能会产生多个segBuf
// 被添加到发送缓冲区链表中. 如果调用成功, 数据就被放入TCB发送缓冲区链表中, 根据滑动窗口的情况,
// 数据可能被传输到网络中, 或在队列中等待传输.

void send_buf_len(int sockfd ,void* data ,int length){
    pthread_mutex_lock(tcbtable[sockfd]->bufMutex);
    segBuf_t *newsegBuf;
    newsegBuf=(segBuf_t*)malloc(sizeof(segBuf_t));
    memcpy((newsegBuf->seg).data,data,length);
    (newsegBuf->seg).header.src_port=tcbtable[sockfd]->client_portNum;
    (newsegBuf->seg).header.dest_port=tcbtable[sockfd]->server_portNum;
    (newsegBuf->seg).header.seq_num=tcbtable[sockfd]->next_seqNum;
    (newsegBuf->seg).header.ack_num=0;
    (newsegBuf->seg).header.length=length;
    (newsegBuf->seg).header.type=DATA;
    (newsegBuf->seg).header.rcv_win=0;
    (newsegBuf->seg).header.checksum=0;
     
    //时间记录
    struct timeval curtime;
	//struct timezone tz;
    gettimeofday(&curtime,NULL);
    newsegBuf->sentTime=curtime.tv_usec;
    newsegBuf->next=NULL;
        
    //pthread_mutex_lock(cli_tcb_pool[sockfd]->bufMutex);
    tcbtable[sockfd]->next_seqNum+=length;
	//printf("next_seqNum : %d\n",cli_tcb_pool[sockfd]->next_seqNum);
    if(tcbtable[sockfd]->sendBufHead==NULL){
        tcbtable[sockfd]->sendBufHead=newsegBuf;
        tcbtable[sockfd]->sendBufTail=newsegBuf;
        tcbtable[sockfd]->sendBufunSent=NULL;
        pthread_t thread;
        int *sub_socketfd;
        sub_socketfd=(int *)malloc(sizeof(int));
        *sub_socketfd=sockfd;
        pthread_create(&thread,NULL,sendBuf_timer,(void*)sub_socketfd);
		//sip_sendseg(tcp_conn,(seg_t*)newsegBuf);
    }
    else{
        tcbtable[sockfd]->sendBufTail->next=newsegBuf;
        tcbtable[sockfd]->sendBufTail=newsegBuf;
        tcbtable[sockfd]->sendBufTail->next=NULL;
    }
    //pthread_mutex_unlock(cli_tcb_pool[sockfd]->bufMutex);
    //temp-=MAX_SEG_LEN;
    //data+=MAX_SEG_LEN;
//	printf("-----------send %d----------\n",tcbtable[sockfd]->unAck_segNum);
	if((tcbtable[sockfd]->unAck_segNum)<GBN_WINDOW){
//		printf("=========send %d=========\n",tcbtable[sockfd]->unAck_segNum);
		sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,&(newsegBuf->seg));
		//cli_tcb_pool[sockfd]->sendBufunSent=cli_tcb_pool[sockfd]->sendBufunSent->next;
		(tcbtable[sockfd]->unAck_segNum)++;
	
	}
	else{
		if(tcbtable[sockfd]->unAck_segNum==GBN_WINDOW){
			if(tcbtable[sockfd]->sendBufunSent==NULL)
                tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufTail;
		}
	}
    pthread_mutex_unlock(tcbtable[sockfd]->bufMutex);
}


int stcp_client_send(int sockfd, void* data, unsigned int length)
{
	if(tcbtable[sockfd]==NULL)
        return -1;
    int temp=length;
    printf("===stcp_client_send %d=====\n",length);
    while(temp>0){
        if(temp>MAX_SEG_LEN){
            send_buf_len(sockfd,data,MAX_SEG_LEN);
            temp -= MAX_SEG_LEN;
            data += MAX_SEG_LEN;
        }
        else{
            if(temp>0){
                send_buf_len(sockfd,data,temp);
                temp=0;
                data += temp;
            }
        }
    }
    return 1;

}

int stcp_client_recv(int sockfd, void* buf, unsigned int length) {
//	printf("===stcp_client_recv===\n");
	if(tcbtable[sockfd] == NULL)
		return -1;
	while(tcbtable[sockfd]->usedBufLen == 0) {
//		printf("usedBufLen: %d\n", tcbtable[sockfd]->usedBufLen);
		sleep(RECVBUF_POLLING_INTERVAL);
	}
	pthread_mutex_lock(tcbtable[sockfd]->bufMutex_recv);//上锁
	/*memcpy(buf, tcbtable[sockfd]->recvBuf, length);
	tcbtable[sockfd]->usedBufLen -= length;
	memcpy(tcbtable[sockfd]->recvBuf, tcbtable[sockfd]->recvBuf + length, tcbtable[sockfd]->usedBufLen);*/

	memcpy(buf, tcbtable[sockfd]->recvBuf, tcbtable[sockfd]->usedBufLen);
	tcbtable[sockfd]->usedBufLen = 0;
	pthread_mutex_unlock(tcbtable[sockfd]->bufMutex_recv);
	return 1;

}
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN段给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
int stcp_client_disconnect(int sockfd) 
{
	seg_t* sendseg=(seg_t*)malloc(sizeof(seg_t));
	memset(sendseg,0,sizeof(seg_t));
	sendseg->header.type=FIN;   
	sendseg->header.src_port=tcbtable[sockfd]->client_portNum;
	sendseg->header.dest_port=tcbtable[sockfd]->server_portNum;
	sendseg->header.seq_num=tcbtable[sockfd]->next_seqNum;
	sendseg->header.length=0;
	sendseg->header.checksum=checksum(sendseg);
	int i=0;
	clock_t start;
	printf("stcp_client_disconnect : %d\n",sendseg->header.type);
	for(;i<FIN_MAX_RETRY;i++){
		sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,sendseg);
        if(tcbtable[sockfd]->state==CONNECTED)
		    tcbtable[sockfd]->state=FINWAIT;
        //printf("State turn to FINWAIT\n");
		start=clock();
		while(1){
            //printf("TCB state:%d\n", cli_tcb_pool[sockfd]->state);
            //sleep(0.1);
			if(tcbtable[sockfd]->state==CLOSED)//收到了FINACK并改变了状态
			{	
				printf("disconnect %d successfully\n", sockfd);
				free(sendseg);											
				return 1;			
			}			
			if(clock()-start>=FIN_TIMEOUT/1000)
				break;//超时								
		}
	}
	if(i>=FIN_MAX_RETRY)  /*超时且超过最大重传次数*/ 
	{
        printf("Out of the FIN_MAX_RETRY\n");
		tcbtable[sockfd]->state=CLOSED;
		return -1;													
	}
	return 0;
}

// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
int stcp_client_close(int sockfd) 
{
	if(tcbtable[sockfd]->state==CLOSED)   /*位于正确的状态，标记条目为NULL，同时释放TCB条目*/
	{
		free(tcbtable[sockfd]);
		tcbtable[sockfd]=NULL;
		return 1;
	}
	return -1;  /*失败时返回-1*/

}

// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明到SIP进程的连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
void* seghandler(void* arg) 
{
	seg_t* recvseg=(seg_t*)malloc(sizeof(seg_t));
	seg_t send_seg;
	memset(recvseg,0,sizeof(seg_t));
	int src_nodeId;
	int len;
	while((len = sip_recvseg(sip_conn,&src_nodeId,recvseg))>=0){
		//printf("%d %d \n",recvseg->header.src_port,recvseg->header.dest_port);
	    if(len==1)
			continue;
		int index_sockfd=-1, i = 0;
	    for(;i < MAX_TRANSPORT_CONNECTIONS; i++){
	    	if(tcbtable[i] != NULL){ 		
    			if(tcbtable[i]->server_portNum==recvseg->header.src_port
	    				&&tcbtable[i]->client_portNum==recvseg->header.dest_port) {
	    			index_sockfd = i;
				break;																				
	    		}/*找到相应的TCB*/								
	    	}						
	    }		
    	if(index_sockfd == -1) {
    		//printf("There is no correct sockfd in pool\n");
    		continue;												
    	}		

    	client_tcb_t *tcb=tcbtable[index_sockfd];
    	switch(tcb->state){
    		case SYNSENT:
    			if(recvseg->header.type==SYNACK) {
    				tcbtable[index_sockfd]->state=CONNECTED;
    				//printf("tcb[%d] state turn to CONNECTED\n", index_sockfd);	
    			}
    			break;
    		case FINWAIT:
    			if(recvseg->header.type==FINACK){
				    tcbtable[index_sockfd]->state=CLOSED;
			    	//printf("tcb[%d] state turn to CLOSED\n", index_sockfd);
			    }
			    break;	
			case CONNECTED:
				//printf("=======get ack %d========\n",recvseg->header.type);
                if(recvseg->header.type == DATAACK){
                    pthread_mutex_lock(tcbtable[index_sockfd]->bufMutex);
                    int seqNum = recvseg->header.seq_num;
					printf("recv DATAACK! ack_seq: %d\n",seqNum);
					while(tcbtable[index_sockfd]->sendBufHead !=NULL){
                   //     printf("seq_num:%d\n",(tcbtable[index_sockfd]->sendBufHead->seg).header.seq_num);
                        if((tcbtable[index_sockfd]->sendBufHead->seg.header.seq_num) < seqNum){
							segBuf_t *p = tcbtable[index_sockfd]->sendBufHead;
							tcbtable[index_sockfd]->sendBufHead=tcbtable[index_sockfd]->sendBufHead->next;
							
						//	printf("unAck_segNum : %d\n",tcbtable[index_sockfd]->unAck_segNum);
							(tcbtable[index_sockfd]->unAck_segNum)--;
							free(p);
							//printf("Free unAck_segNum!!!!\n");
							//(tcb->unAck_segNum)--;
						}
						else break;
					}
					if(tcbtable[index_sockfd]->sendBufHead == NULL){
						tcbtable[index_sockfd]->sendBufunSent = NULL;
						tcbtable[index_sockfd]->sendBufTail = NULL;
					    pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex);
						break;
					}
                    while(tcbtable[index_sockfd]->unAck_segNum<GBN_WINDOW&&
							tcbtable[index_sockfd]->sendBufunSent!=NULL){
						struct timeval curtime;
						//struct timezone tz;
						gettimeofday(&curtime,NULL);
						tcbtable[index_sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
                        sip_sendseg(sip_conn,tcbtable[index_sockfd]->server_nodeID,&(tcbtable[index_sockfd]->sendBufunSent->seg));
                        (tcbtable[index_sockfd]->unAck_segNum)++;
						//printf("unAck_segNum : %d\n",tcbtable[index_sockfd]->unAck_segNum);
                        tcbtable[index_sockfd]->sendBufunSent=tcbtable[index_sockfd]->sendBufunSent->next;
                    }
					pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex);
				}
				else if(recvseg->header.type == DATA) {//接收到数据
					pthread_mutex_lock(tcbtable[index_sockfd]->bufMutex_recv);
					//printf("recv DATA seq:%d, excepted seq:%d\n", recvseg->header.seq_num, tcbtable[index_sockfd]->expect_seqNum);
					if(recvseg->header.seq_num == tcbtable[index_sockfd]->expect_seqNum) {
						int usedLen=tcbtable[index_sockfd]->usedBufLen;
						tcbtable[index_sockfd]->usedBufLen += recvseg->header.length;
						if(tcbtable[index_sockfd]->usedBufLen > RECEIVE_BUF_SIZE) {
							//printf("buffer overflow!\n");
							break;
						}
						else {
							memcpy((tcbtable[index_sockfd]->recvBuf) + usedLen, recvseg->data, recvseg->header.length);
							tcbtable[index_sockfd]->expect_seqNum += recvseg->header.length;
							memcpy(&send_seg, &recvseg, sizeof(recvseg));
							send_seg.header.dest_port = tcbtable[index_sockfd]->server_portNum;
							send_seg.header.src_port = tcbtable[index_sockfd]->client_portNum;
							send_seg.header.seq_num = tcbtable[index_sockfd]->expect_seqNum;
							send_seg.header.length = 0;
							send_seg.header.type = DATAACK;
							sip_sendseg(sip_conn, tcbtable[index_sockfd]->server_nodeID, &send_seg);
						}
						pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_recv);
					}
					else {
						seg_t *seg = (seg_t *)malloc(sizeof(seg_t));
						memset(seg, 0, sizeof(seg_t));
						seg->header.dest_port = tcbtable[index_sockfd]->server_portNum;
						seg->header.src_port = tcbtable[index_sockfd]->client_portNum;
						seg->header.seq_num = tcbtable[index_sockfd]->expect_seqNum;
						seg->header.length = 0;
						seg->header.type = DATAACK;
						sip_sendseg(sip_conn, tcbtable[index_sockfd]->server_nodeID, seg);
						pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_recv);
					}
				}
				break;

	    	default:	
				//printf("====error ====\n");
	   		break;	
		}				
    }
	free(recvseg);
	pthread_exit(NULL);
	/*重叠网络链接已经关闭，线程终止*/
	return NULL;
}


//这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
//如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
//当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
void* sendBuf_timer(void* clienttcb) 
{
    int sentTime;
    int sockfd=*(int*)clienttcb;
    while(tcbtable[sockfd]->sendBufHead!=NULL){
		pthread_mutex_lock(tcbtable[sockfd]->bufMutex);
		printf("sendbuf_timer!!!!!!\n");
        struct timeval curtime;
		//struct timezone tz;
        gettimeofday(&curtime,NULL);
        sentTime=curtime.tv_usec;
		//printf("time layout ----------: %d \n",sentTime-cli_tcb_pool[sockfd]->sendBufHead->sentTime);
        if(tcbtable[sockfd]->sendBufHead!=NULL&&(sentTime-tcbtable[sockfd]->sendBufHead->sentTime)>DATA_TIMEOUT/1000){
           // printf("!!!!!!!!!!!!!!!!!!!!!\n");
			//pthread_mutex_lock(cli_tcb_pool[sockfd]->bufMutex);
            tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufHead;
            tcbtable[sockfd]->unAck_segNum=0;
            while(tcbtable[sockfd]->unAck_segNum<GBN_WINDOW){
                if(tcbtable[sockfd]->sendBufunSent!=NULL){
					printf("----------------resent DATA--------------\n");
                    sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,&(tcbtable[sockfd]->sendBufunSent->seg));
					//gettimeofday(&curtime ,&tz);
					//cli_tcb_pool[sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
                    (tcbtable[sockfd]->unAck_segNum)++;
					gettimeofday(&curtime,NULL);
					tcbtable[sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
                    tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufunSent->next;
                }
                else break;
            }
            //pthread_mutex_unlock(cli_tcb_pool[sockfd]->bufMutex);
        }
        pthread_mutex_unlock(tcbtable[sockfd]->bufMutex);
        usleep(SENDBUF_POLLING_INTERVAL/1000);
		//sleep(1);
	}
//	printf("end ------!!!!\n");
	pthread_exit(NULL);
	return NULL;
}

