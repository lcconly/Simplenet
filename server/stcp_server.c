//ÎÄŒþÃû: server/stcp_server.c
//
//ÃèÊö: ÕâžöÎÄŒþ°üº¬STCP·þÎñÆ÷œÓ¿ÚÊµÏÖ.
//
//ŽŽœšÈÕÆÚ: 2013Äê1ÔÂ
#define _BSD_SOURCE
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"
#include "stdbool.h"

//ÉùÃ÷tcbtableÎªÈ«ŸÖ±äÁ¿
server_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//ÉùÃ÷µœSIPœø³ÌµÄÁ¬œÓÎªÈ«ŸÖ±äÁ¿
int sip_conn;
clock_t start[MAX_TRANSPORT_CONNECTIONS];

/*********************************************************************/
//
//STCP APIÊµÏÖ
//
/*********************************************************************/

// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void stcp_server_init(int conn)
{
    printf("===stcp_server_init===\n");
    int i = 0;
    for(; i < MAX_TRANSPORT_CONNECTIONS; i++)
        tcbtable[i] = NULL;
    sip_conn = conn;

    pthread_t thread = (pthread_t)conn;
    int rc = pthread_create(&thread, NULL,seghandler, NULL);
    if(rc)
    {
        perror("Problem in creating pthread\n");
    }
    return;
}

// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port.
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接.
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_sock(unsigned int server_port)
{
    printf("===stcp_server_sock===\n");
    int i = 0;
    for(; i < MAX_TRANSPORT_CONNECTIONS; i++)
    {
        if(tcbtable[i] == NULL)
        {
            server_tcb_t *p = (server_tcb_t*)malloc(sizeof(server_tcb_t));
            p->state = CLOSED;
            p->server_portNum = server_port;
            p->server_nodeID=topology_getMyNodeID();
            p->expect_seqNum=0;
            //p->ack_Num=0;
            p->recvBuf=(char *)malloc(RECEIVE_BUF_SIZE);
            p->usedBufLen=0;
            p->bufMutex_recv=malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(p->bufMutex_recv,NULL);
            p->next_seqNum=0;
            p->bufMutex_send=malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(p->bufMutex_send,NULL);
            p->sendBufHead=NULL;
            p->sendBufTail=NULL;
            p->sendBufunSent=NULL;
            p->unAck_segNum=0;
            tcbtable[i] = p;
            return i;
        }
    }
    return -1;
}

// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后启动定时器进入忙等待直到TCB状态转换为CONNECTED
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_accept(int sockfd)
{
    printf("===stcp_server_accept===\n");
    tcbtable[sockfd]->state = LISTENING;
    while(1)
    {
        sleep(RECVBUF_POLLING_INTERVAL/1000);
        if(tcbtable[sockfd]->state == CONNECTED)
            break;
    }
    return 1;
}
// 接收来自STCP客户端的数据. 请回忆STCP使用的是单向传输, 数据从客户端发送到服务器端.
// 信号/控制信息(如SYN, SYNACK等)则是双向传递. 这个函数每隔RECVBUF_POLLING_INTERVAL时间
// 就查询接收缓冲区, 直到等待的数据到达, 它然后存储数据并返回1. 如果这个函数失败, 则返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length)
{
    printf("===stcp_server_recv===\n");
    //printf("before recv!\n");
    if(tcbtable[sockfd]==NULL)
        return -1;
    while(tcbtable[sockfd]->usedBufLen == 0)
    {
		printf("usedBufLen: %d\n", tcbtable[sockfd]->usedBufLen);
        sleep(RECVBUF_POLLING_INTERVAL);
    }
    pthread_mutex_lock(tcbtable[sockfd]->bufMutex_recv);
  /*  memcpy(buf,tcbtable[sockfd]->recvBuf,length);
    (tcbtable[sockfd]->usedBufLen)-=length;
    memcpy(tcbtable[sockfd]->recvBuf,tcbtable[sockfd]->recvBuf+length,tcbtable[sockfd]->usedBufLen);*/

	memcpy(buf, tcbtable[sockfd]->recvBuf, tcbtable[sockfd]->usedBufLen);
	tcbtable[sockfd]->usedBufLen = 0;
    pthread_mutex_unlock(tcbtable[sockfd]->bufMutex_recv);
    //printf("after recv!\n");
    return 1;
}
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
int stcp_server_close(int sockfd)
{
    while(1)
    {
        if(tcbtable[sockfd]->state == CLOSEWAIT)
        {
            if(clock()-start[sockfd]>=CLOSEWAIT_TIMEOUT*10000)
            {
                tcbtable[sockfd]->state = CLOSED;
                break;
            }
        }
    }
    if(tcbtable[sockfd]->state == CLOSED)
    {
        free(tcbtable[sockfd]);
        tcbtable[sockfd] = NULL;
        printf("TCB[%d] close successfully\n", sockfd);
        return 1;
    }
    return -1;

}

// ÕâÊÇÓÉstcp_server_init()Æô¶¯µÄÏß³Ì. ËüŽŠÀíËùÓÐÀŽ×Ô¿Í»§¶ËµÄœøÈëÊýŸÝ. seghandler±»ÉèŒÆÎªÒ»žöµ÷ÓÃsip_recvseg()µÄÎÞÇîÑ­»·,
// Èç¹ûsip_recvseg()Ê§°Ü, ÔòËµÃ÷µœSIPœø³ÌµÄÁ¬œÓÒÑ¹Ø±Õ, Ïß³Ìœ«ÖÕÖ¹. žùŸÝSTCP¶ÎµœŽïÊ±Á¬œÓËùŽŠµÄ×ŽÌ¬, ¿ÉÒÔ²ÉÈ¡²»Í¬µÄ¶¯×÷.
// Çë²é¿Ž·þÎñ¶ËFSMÒÔÁËœâžü¶àÏžœÚ.
bool send_Ack(seg_t *seg, int conn, int type, int sockfd)
{
    seg->header.src_port = tcbtable[sockfd]->server_portNum;
    seg->header.dest_port = tcbtable[sockfd]->client_portNum;
    seg->header.type = type;
    if(sip_sendseg(conn, tcbtable[sockfd]->client_nodeID,seg) == 0)
        return true;
    else
        return false;
}

int Find_TCB(int src_port, int dest_port)
{
    int i = 0;
    for(; i < MAX_TRANSPORT_CONNECTIONS; i++)
    {
        if(tcbtable[i] != NULL)
        {
            if(src_port == tcbtable[i]->client_portNum
                    && dest_port == tcbtable[i]->server_portNum)
                return i;
        }
    }
    return -1;

}



void send_buf_len(int sockfd ,void* data ,int length)
{
    pthread_mutex_lock(tcbtable[sockfd]->bufMutex_send);
    segBuf_t *newsegBuf;
    newsegBuf=(segBuf_t*)malloc(sizeof(segBuf_t));
    memcpy((newsegBuf->seg).data,data,length);
    (newsegBuf->seg).header.dest_port=tcbtable[sockfd]->client_portNum;
    (newsegBuf->seg).header.src_port=tcbtable[sockfd]->server_portNum;
    (newsegBuf->seg).header.seq_num=tcbtable[sockfd]->next_seqNum;
    (newsegBuf->seg).header.ack_num=0;
    (newsegBuf->seg).header.length=length;
    (newsegBuf->seg).header.type=DATA;
    (newsegBuf->seg).header.rcv_win=0;
    (newsegBuf->seg).header.checksum=0;

    struct timeval curtime;
    //struct timezone tz;
    gettimeofday(&curtime,NULL);
    newsegBuf->sentTime=curtime.tv_usec;
    newsegBuf->next=NULL;

    //pthread_mutex_lock(cli_tcb_pool[sockfd]->bufMutex_send);
    tcbtable[sockfd]->next_seqNum+=length;
    //printf("next_seqNum : %d\n",cli_tcb_pool[sockfd]->next_seqNum);
    if(tcbtable[sockfd]->sendBufHead==NULL)
    {
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
    else
    {
        tcbtable[sockfd]->sendBufTail->next=newsegBuf;
        tcbtable[sockfd]->sendBufTail=newsegBuf;
        tcbtable[sockfd]->sendBufTail->next=NULL;
    }
    printf("-----------send %d----------\n",tcbtable[sockfd]->unAck_segNum);
    if((tcbtable[sockfd]->unAck_segNum)<GBN_WINDOW)
    {
        printf("=========send %d=========\n",tcbtable[sockfd]->unAck_segNum);
        sip_sendseg(sip_conn,tcbtable[sockfd]->client_nodeID,&(newsegBuf->seg));
        (tcbtable[sockfd]->unAck_segNum)++;

    }
    else
    {
        if(tcbtable[sockfd]->unAck_segNum==GBN_WINDOW)
        {
            if(tcbtable[sockfd]->sendBufunSent==NULL)
                tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufTail;
        }
    }
    pthread_mutex_unlock(tcbtable[sockfd]->bufMutex_send);
}


int stcp_server_send(int sockfd, void* data, unsigned int length)
{
    if(tcbtable[sockfd]==NULL)
        return -1;
    int temp=length;
    printf("===stcp_client_send %d=====\n",length);
    while(temp>0)
    {
        if(temp>MAX_SEG_LEN)
        {
            send_buf_len(sockfd,data,MAX_SEG_LEN);
            temp-= MAX_SEG_LEN;
            data += MAX_SEG_LEN;
        }
        else
        {
            if(temp>0)
            {
                send_buf_len(sockfd,data,temp);
                temp=0;
                data += temp;
            }
        }
    }
    return 1;
}



// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环,
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//
void* seghandler(void* arg)
{
    int arg_conn = sip_conn;
    seg_t recv_seg;
    seg_t send_seg;
    int len;
    int src_nodeId;
    while(1)
    {
        memset(&send_seg,0,sizeof(seg_t));
        if((len=sip_recvseg(arg_conn,&src_nodeId, &(recv_seg))) == -1) //连接关闭
        {
            printf("SON has been closed\n");
            break;
        }
        else if(len==1)  //数据包丢失
        {
            continue;
        }
        else  //正常
        {
            int index_sockfd = -1, i = 0;
            switch((recv_seg).header.type)
            {
            case SYN:
                for(; i < MAX_TRANSPORT_CONNECTIONS; i++)
                {
                    if(tcbtable[i] != NULL)
                    {
                        if(recv_seg.header.dest_port == tcbtable[i]->server_portNum)
                        {
                            index_sockfd = i;
                            break;
                        }
                    }
                }
                if(index_sockfd == -1)
                {
                    printf("no correct sockfd in tcbtable\n");
                    break;
                }
                if(tcbtable[index_sockfd]->state == LISTENING)
                {
                    tcbtable[index_sockfd]->state = CONNECTED;
                    tcbtable[index_sockfd]->client_portNum = recv_seg.header.src_port;
                    tcbtable[index_sockfd]->client_nodeID=src_nodeId;
                    if(send_Ack(&send_seg, arg_conn, SYNACK, index_sockfd))
                        printf("Send SYNACK Successfully\n");
                }
                else if(tcbtable[index_sockfd]->state == CONNECTED)
                {
                    if(send_Ack(&send_seg, arg_conn, SYNACK, index_sockfd))
                        printf("Send SYNACK Successfully\n");
                    //send SYNACK
                }
                break;
            case FIN:
                index_sockfd = Find_TCB(recv_seg.header.src_port, recv_seg.header.dest_port);
                if(index_sockfd == -1)
                {
                    printf("no correct sockfd in tcbtable\n");
                    break;
                }
                printf("FINACK->sockfd:%d\n", index_sockfd);
                if(tcbtable[index_sockfd]->state == CONNECTED)
                {
                    tcbtable[index_sockfd]->state = CLOSEWAIT;
                    if(send_Ack(&send_seg, arg_conn, FINACK, index_sockfd))
                        printf("Send FINACK Successfully\n");
                    //send FINACK
                }
                else if(tcbtable[index_sockfd]->state == CLOSEWAIT)
                {
                    if(send_Ack(&send_seg, arg_conn, FINACK, index_sockfd))
                        printf("Send FINACK Successfully\n");
                    //send FINACK
                }
                break;

            case DATA:
                index_sockfd = Find_TCB(recv_seg.header.src_port, recv_seg.header.dest_port);
                if(tcbtable[index_sockfd]->state==CONNECTED)
                {
                    printf("============recv DATA=============\n");
                    if(index_sockfd == -1)
                    {
                        printf("No correct sockfd in tcbtable\n");
                        break;
                    }
                    pthread_mutex_lock(tcbtable[index_sockfd]->bufMutex_recv);
                    printf("recv DATA! seq: %d; expected seq: %d\n",recv_seg.header.seq_num,tcbtable[index_sockfd]->expect_seqNum);
                    if(recv_seg.header.seq_num == tcbtable[index_sockfd]->expect_seqNum)
                    {
                        int usedLen=tcbtable[index_sockfd]->usedBufLen;
                        tcbtable[index_sockfd]->usedBufLen+=recv_seg.header.length;
                        if(tcbtable[index_sockfd]->usedBufLen>RECEIVE_BUF_SIZE)
                        {
                            printf("buffer overflow!\n");
                            break;
                        }
                        else
                        {
                            memcpy((tcbtable[index_sockfd]->recvBuf)+usedLen,recv_seg.data,recv_seg.header.length);
                            tcbtable[index_sockfd]->expect_seqNum+=recv_seg.header.length;
                            memcpy(&send_seg,&recv_seg,sizeof(recv_seg));
                            send_seg.header.dest_port=tcbtable[index_sockfd]->client_portNum;
                            send_seg.header.src_port=tcbtable[index_sockfd]->server_portNum;
                            send_seg.header.seq_num=tcbtable[index_sockfd]->expect_seqNum;
                            send_seg.header.length=0;

                            //send_seg.header.ack_num=recv_seg.header.seq_num;
                            //printf("seq num: %d        \n",recv_seg.header.seq_num);
                            send_seg.header.type=DATAACK;
                            //send_seg.header.length=0;
                            printf("Send a DATAACK!\n");
                            printf("Seq_num : %d\n",send_seg.header.seq_num);
                            sip_sendseg(arg_conn,tcbtable[index_sockfd]->client_nodeID,&send_seg);

						//	int n = 0;
						//	ResWritePacket(index_sockfd, tcbtable[index_sockfd]->recvBuf, n);//解析发送数据

                        }
                        pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_recv);
                        printf("=============end recv===========\n");
                        break;
                    }
                    else
                    {
                        seg_t *seg=(seg_t *)malloc(sizeof(seg_t));
                        memset(seg,0,sizeof(seg_t));
                        seg->header.src_port=tcbtable[index_sockfd]->server_portNum;
                        seg->header.dest_port=recv_seg.header.src_port;
                        seg->header.seq_num=tcbtable[index_sockfd]->expect_seqNum;
                        seg->header.type=DATAACK;
                        seg->header.length=0;
                        sip_sendseg(arg_conn,tcbtable[index_sockfd]->client_nodeID,seg);
                        printf("resend DATAACK!!!!\n");
                        pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_recv);
                    }
                }
                
                break;
                /*以下部分是新加的，用来处理从客户端收到的对服务器所发数据块的确认*/
            case DATAACK:
                index_sockfd = Find_TCB(recv_seg.header.src_port, recv_seg.header.dest_port); //找到TCB块对应的索引
                if(tcbtable[index_sockfd]->state==CONNECTED)
                {
                    pthread_mutex_lock(tcbtable[index_sockfd]->bufMutex_send);  // 发送缓冲区上锁
                    int seqNum=recv_seg.header.seq_num;
                    printf("recv DATAACK! ack_seq: %d\n",seqNum);
                    while(tcbtable[index_sockfd]->sendBufHead!=NULL)
                    {
                        printf("!!!!!!!! %d\n",(tcbtable[index_sockfd]->sendBufHead->seg).header.seq_num);
                        if((tcbtable[index_sockfd]->sendBufHead->seg.header.seq_num)<seqNum)
                        {
                            segBuf_t *p=tcbtable[index_sockfd]->sendBufHead;
                            tcbtable[index_sockfd]->sendBufHead=tcbtable[index_sockfd]->sendBufHead->next;
                            printf("unAck_segNum : %d \n",tcbtable[index_sockfd]->unAck_segNum);
                            (tcbtable[index_sockfd]->unAck_segNum)--;
                            free(p);
                        }
                        else break;
                    }
                    if(tcbtable[index_sockfd]->sendBufHead==NULL)
                    {
                        tcbtable[index_sockfd]->sendBufunSent=NULL;
                        tcbtable[index_sockfd]->sendBufTail=NULL;
                        pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_send);
                        break;
                    }
                    while(tcbtable[index_sockfd]->unAck_segNum<GBN_WINDOW&&
                            tcbtable[index_sockfd]->sendBufunSent!=NULL)
                    {
                        struct timeval curtime;
                        //struct timezone tz;
                        gettimeofday(&curtime,NULL);
                        tcbtable[index_sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
			//这个函数才是真正的发出去
                        sip_sendseg(sip_conn,tcbtable[index_sockfd]->server_nodeID,&(tcbtable[index_sockfd]->sendBufunSent->seg));
                        (tcbtable[index_sockfd]->unAck_segNum)++;
                        printf("unAck_segNum : %d \n",tcbtable[index_sockfd]->unAck_segNum);
                        tcbtable[index_sockfd]->sendBufunSent=tcbtable[index_sockfd]->sendBufunSent->next;
                    }
                    pthread_mutex_unlock(tcbtable[index_sockfd]->bufMutex_send);
                }
                break;
            default:
                break;
            }
        }
    }
    pthread_exit(0);
}

/*这个是新加的*/


//当发送缓冲区非空时，该线程运行，每隔一段时间就去查询第一个已发送但是未被确认段，如果超时(当前时间和段中有记录时间之差大于间隔)，
//发送缓冲区中所有已发送但未被确认的数据段都会被重发
//当函数stcp_client_send()发现当前发送缓冲区为空，就会启动该线程，而当发送缓冲区所有数据段都被确认了，该线程自己终止执行
void* sendBuf_timer(void* clienttcb)
{

    int sentTime;
    int sockfd=*(int*)clienttcb;
    while(tcbtable[sockfd]->sendBufHead!=NULL)
    {
        pthread_mutex_lock(tcbtable[sockfd]->bufMutex_send);
        printf("sendbuf_timer!!!!!!\n");
        struct timeval curtime;
        //struct timezone tz;
        gettimeofday(&curtime,NULL);
        sentTime=curtime.tv_usec;
        //printf("time layout ----------: %d \n",sentTime-cli_tcb_pool[sockfd]->sendBufHead->sentTime);
        if(tcbtable[sockfd]->sendBufHead!=NULL&&(sentTime-tcbtable[sockfd]->sendBufHead->sentTime)>DATA_TIMEOUT/1000)
        {
            printf("!!!!!!!!!!!!!!!!!!!!!\n");
            //pthread_mutex_lock(cli_tcb_pool[sockfd]->bufMutex);
            tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufHead;
            tcbtable[sockfd]->unAck_segNum=0;
            while(tcbtable[sockfd]->unAck_segNum<GBN_WINDOW)
            {
                if(tcbtable[sockfd]->sendBufunSent!=NULL)
                {
                    printf("----------------resent DATA--------------\n");
                    sip_sendseg(sip_conn,tcbtable[sockfd]->client_nodeID,&(tcbtable[sockfd]->sendBufunSent->seg));
                    //gettimeofday(&curtime ,&tz);
                    //cli_tcb_pool[sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
                    (tcbtable[sockfd]->unAck_segNum)++;
                    gettimeofday(&curtime,NULL);
                    tcbtable[sockfd]->sendBufunSent->sentTime=curtime.tv_usec;
                    tcbtable[sockfd]->sendBufunSent=tcbtable[sockfd]->sendBufunSent->next;
                }
                else break;
            }
        }
        pthread_mutex_unlock(tcbtable[sockfd]->bufMutex_send);
        usleep(SENDBUF_POLLING_INTERVAL/1000);
        //sleep(1);
    }
    printf("end ------!!!!\n");
    pthread_exit(NULL);
    return NULL;
}

