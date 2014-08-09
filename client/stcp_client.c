//�ļ���: client/stcp_client.c
//
//����: ����ļ�����STCP�ͻ��˽ӿ�ʵ�� 
//
//��������: 2013��1��
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

//����tcbtableΪȫ�ֱ���
client_tcb_t* tcbtable[MAX_TRANSPORT_CONNECTIONS];
//������SIP���̵�TCP����Ϊȫ�ֱ���
int sip_conn;

/*********************************************************************/
//
//STCP APIʵ��
//
/*********************************************************************/

// ���������ʼ��TCB��, ��������Ŀ���ΪNULL.  
// �������TCP�׽���������conn��ʼ��һ��STCP���ȫ�ֱ���, �ñ�����Ϊsip_sendseg��sip_recvseg���������.
// ���, �����������seghandler�߳�����������STCP��. �ͻ���ֻ��һ��seghandler.
void stcp_client_init(int conn) 
{
	printf("===stcp_client_init===\n");
	int i=0;
	for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
		tcbtable[i]=NULL;                /*��ʼ��TCB��ΪNULL*/
	sip_conn=conn;
	/*����seghandler�߳�*/
	pthread_t tid;
	if(pthread_create(&tid, NULL, seghandler, NULL) == -1)
		perror("Start the thread unsuccessfully!\n");
	return;

}

// ����������ҿͻ���TCB�����ҵ���һ��NULL��Ŀ, Ȼ��ʹ��malloc()Ϊ����Ŀ����һ���µ�TCB��Ŀ.
// ��TCB�е������ֶζ�����ʼ��. ����, TCB state������ΪCLOSED���ͻ��˶˿ڱ�����Ϊ�������ò���client_port. 
// TCB������Ŀ��������Ӧ��Ϊ�ͻ��˵����׽���ID�������������, �����ڱ�ʶ�ͻ��˵�����. 
// ���TCB����û����Ŀ����, �����������-1.
int stcp_client_sock(unsigned int client_port) 
{

	printf("===stcp_client_sock===\n");
	int i=0;
	for(;i<MAX_TRANSPORT_CONNECTIONS;i++)
		if(tcbtable[i]==NULL)
			break;                          /*�ҵ���һ��NULL��Ŀ*/
	if(i==MAX_TRANSPORT_CONNECTIONS)
		return -1;						/*TCB��������Ŀ���ã��򷵻�-1*/
	tcbtable[i]=(client_tcb_t*)malloc(sizeof(client_tcb_t));
	memset(tcbtable[i],0,sizeof(client_tcb_t));
	/*��ʼ��������*/
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
	//�ͻ������ݽ��ջ�����
	tcbtable[i]->recvBuf = (char* )malloc(RECEIVE_BUF_SIZE);
	tcbtable[i]->usedBufLen = 0;
	tcbtable[i]->bufMutex_recv = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(tcbtable[i]->bufMutex_recv, NULL);
	return i;

}

// ��������������ӷ�����. �����׽���ID, �������ڵ�ID�ͷ������Ķ˿ں���Ϊ�������. �׽���ID�����ҵ�TCB��Ŀ.  
// �����������TCB�ķ������ڵ�ID�ͷ������˿ں�,  Ȼ��ʹ��sip_sendseg()����һ��SYN�θ�������.  
// �ڷ�����SYN��֮��, һ����ʱ��������. �����SYNSEG_TIMEOUTʱ��֮��û���յ�SYNACK, SYN �ν����ش�. 
// ����յ���, �ͷ���1. ����, ����ش�SYN�Ĵ�������SYN_MAX_RETRY, �ͽ�stateת����CLOSED, ������-1.
int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) 
{
	printf("===stcp_client_connect===\n");
	/*sockfd��ȡֵ��Χ���ڵ��ô˺���ʱ���м�飬��������Ͳ���ȷ��*/
	tcbtable[sockfd]->server_portNum=server_port;
	tcbtable[sockfd]->server_nodeID=nodeID;
	seg_t* sendseg=(seg_t*)malloc(sizeof(seg_t));
	memset(sendseg,0,sizeof(seg_t));
	int i=0;
	clock_t start;  /**/
	for(;i<=SYN_MAX_RETRY;i++){    /*��һ�η��ͺ�֮������ش�����*/
		sendseg->header.src_port=tcbtable[sockfd]->client_portNum;
		sendseg->header.dest_port=server_port;
		sendseg->header.length=0;
		sendseg->header.type = SYN;
		sip_sendseg(sip_conn,tcbtable[sockfd]->server_nodeID,sendseg);  /*����SYN��������*/
		tcbtable[sockfd]->state=SYNSENT;
		printf("client send SYN!!\n");
		start =clock();
		while(1){
			if(tcbtable[sockfd]->state == CONNECTED){  /*�յ�����SYNACK���ݰ�*/
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
		return -1;   /*��������ش�����*/
	}
}

// �������ݸ�STCP������. �������ʹ���׽���ID�ҵ�TCB���е���Ŀ.
// Ȼ����ʹ���ṩ�����ݴ���segBuf, �������ӵ����ͻ�����������.
// ������ͻ������ڲ�������֮ǰΪ��, һ����Ϊsendbuf_timer���߳̾ͻ�����.
// ÿ��SENDBUF_ROLLING_INTERVALʱ���ѯ���ͻ������Լ���Ƿ��г�ʱ�¼�����. 
// ��������ڳɹ�ʱ����1�����򷵻�-1. 
// stcp_client_send��һ����������������.
// ��Ϊ�û����ݱ���ƬΪ�̶���С��STCP��, ����һ��stcp_client_send���ÿ��ܻ�������segBuf
// ����ӵ����ͻ�����������. ������óɹ�, ���ݾͱ�����TCB���ͻ�����������, ���ݻ������ڵ����,
// ���ݿ��ܱ����䵽������, ���ڶ����еȴ�����.

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
     
    //ʱ���¼
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
	pthread_mutex_lock(tcbtable[sockfd]->bufMutex_recv);//����
	/*memcpy(buf, tcbtable[sockfd]->recvBuf, length);
	tcbtable[sockfd]->usedBufLen -= length;
	memcpy(tcbtable[sockfd]->recvBuf, tcbtable[sockfd]->recvBuf + length, tcbtable[sockfd]->usedBufLen);*/

	memcpy(buf, tcbtable[sockfd]->recvBuf, tcbtable[sockfd]->usedBufLen);
	tcbtable[sockfd]->usedBufLen = 0;
	pthread_mutex_unlock(tcbtable[sockfd]->bufMutex_recv);
	return 1;

}
// ����������ڶϿ���������������. �����׽���ID��Ϊ�������. �׽���ID�����ҵ�TCB���е���Ŀ.  
// �����������FIN�θ�������. �ڷ���FIN֮��, state��ת����FINWAIT, ������һ����ʱ��.
// ��������ճ�ʱ֮ǰstateת����CLOSED, �����FINACK�ѱ��ɹ�����. ����, ����ھ���FIN_MAX_RETRY�γ���֮��,
// state��ȻΪFINWAIT, state��ת����CLOSED, ������-1.
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
			if(tcbtable[sockfd]->state==CLOSED)//�յ���FINACK���ı���״̬
			{	
				printf("disconnect %d successfully\n", sockfd);
				free(sendseg);											
				return 1;			
			}			
			if(clock()-start>=FIN_TIMEOUT/1000)
				break;//��ʱ								
		}
	}
	if(i>=FIN_MAX_RETRY)  /*��ʱ�ҳ�������ش�����*/ 
	{
        printf("Out of the FIN_MAX_RETRY\n");
		tcbtable[sockfd]->state=CLOSED;
		return -1;													
	}
	return 0;
}

// �����������free()�ͷ�TCB��Ŀ. ��������Ŀ���ΪNULL, �ɹ�ʱ(��λ����ȷ��״̬)����1,
// ʧ��ʱ(��λ�ڴ����״̬)����-1.
int stcp_client_close(int sockfd) 
{
	if(tcbtable[sockfd]->state==CLOSED)   /*λ����ȷ��״̬�������ĿΪNULL��ͬʱ�ͷ�TCB��Ŀ*/
	{
		free(tcbtable[sockfd]);
		tcbtable[sockfd]=NULL;
		return 1;
	}
	return -1;  /*ʧ��ʱ����-1*/

}

// ������stcp_client_init()�������߳�. �������������Է������Ľ����. 
// seghandler�����Ϊһ������sip_recvseg()������ѭ��. ���sip_recvseg()ʧ��, ��˵����SIP���̵������ѹر�,
// �߳̽���ֹ. ����STCP�ε���ʱ����������״̬, ���Բ�ȡ��ͬ�Ķ���. ��鿴�ͻ���FSM���˽����ϸ��.
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
	    		}/*�ҵ���Ӧ��TCB*/								
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
				else if(recvseg->header.type == DATA) {//���յ�����
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
	/*�ص����������Ѿ��رգ��߳���ֹ*/
	return NULL;
}


//����̳߳�����ѯ���ͻ������Դ�����ʱ�¼�. ������ͻ������ǿ�, ��Ӧһֱ����.
//���(��ǰʱ�� - ��һ���ѷ��͵�δ��ȷ�϶εķ���ʱ��) > DATA_TIMEOUT, �ͷ���һ�γ�ʱ�¼�.
//����ʱ�¼�����ʱ, ���·��������ѷ��͵�δ��ȷ�϶�. �����ͻ�����Ϊ��ʱ, ����߳̽���ֹ.
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

