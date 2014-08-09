// 文件名 pkt.c
// 创建日期: 2013年1月

#include "pkt.h"
#include <string.h>
#include "sys/socket.h"
#include "stdlib.h"
#include "stdio.h"
#include <errno.h>
#include "stdbool.h"
#define PKTSTART1 0
#define PKTSTART2 1
// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	char *start = "!&", *end = "!#";
    char *buf;
    buf=(char *)malloc(8+sizeof(sip_hdr_t)+pkt->header.length);
	memcpy(buf,start,2);
	memcpy(buf+2,&nextNodeID,4);
    memcpy(buf+6,pkt,sizeof(sip_hdr_t)+pkt->header.length);
    memcpy(buf+6+sizeof(sip_hdr_t)+pkt->header.length,end,2);
    if(send(son_conn, buf, 8+sizeof(sip_hdr_t)+pkt->header.length, 0) < 0){
        printf("========son_sendpkt fail========\n");
        printf("son send pkt error !!!!\n");
        return -1;
	}   
    printf("========son_sendpkt success========\n");
	printf("      nextNodeID: %d\n",nextNodeID);
    printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
    printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
    printf("      header length: %d\n",(pkt->header).length);
    printf("      header type: %d\n",(pkt->header).type);
    return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int readn_1( int fd, char *bp, size_t len)
{
	int cnt;
	int rc;

	cnt = len;
	while ( cnt > 0 )
	{
		rc = recv( fd, bp, cnt, 0 );
		if ( rc < 0 )				/* read error? */
		{
			if ( errno == EINTR )	/* interrupted? */
				continue;			/* restart the read */
			return -1;				/* return error */
		}
		if ( rc == 0 )				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
} 

int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	char data[1];
    int n;
    int state = PKTSTART1;
    while((n = readn_1(son_conn,data,1))>0){
        if(data[0] == '!' && state == PKTSTART1){
            state = PKTSTART2;
        }
        else if(data[0] == '&' && state == PKTSTART2){
            if((n = readn_1(son_conn,(char *)&pkt->header,sizeof(sip_hdr_t))) > 0){
                int len = pkt->header.length;
                printf("len : %d\n",len);
                char *buf = (char *)malloc(len+2);
                memset(buf,0,len+2);
                if((n = readn_1(son_conn,buf,len+2))>0){
                    printf("end :   --------   %c %c\n",buf[len],buf[len+1]);
                    if(buf[len]  == '!' && buf[len+1] == '#'){
                        memset(pkt->data,0,MAX_PKT_LEN);
                        memcpy(pkt->data,buf,len);
					    printf("========son_recvpkt success========\n");
					    printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
    					printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
  						printf("      header length: %d\n",(pkt->header).length);
  						printf("      header type: %d\n",(pkt->header).type);
 					    return 1;
                    }
                    else
                        return -1;
                }
                else
                    return -1;
            }
            else
                return -1;
        }
        else
            state = PKTSTART1;
    }
    return -1; 	
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	char data[1];
    int n;
    int state = PKTSTART1;
    while((n = readn_1(sip_conn,data,1))>0){
        if(data[0] == '!' && state == PKTSTART1){
            state = PKTSTART2;
        }
        else if(data[0] == '&' && state == PKTSTART2){
			if((n = readn_1(sip_conn,(char *)nextNode,4)) > 0){
		        if((n = readn_1(sip_conn,(char *)&pkt->header,sizeof(sip_hdr_t))) > 0){
		            int len = pkt->header.length;
		            printf("len : %d\n",len);
		            char *buf = (char *)malloc(len+2);
		            memset(buf,0,len+2);
		            if((n = readn_1(sip_conn,buf,len+2))>0){
		                printf("end :   --------   %c %c\n",buf[len],buf[len+1]);
		                if(buf[len]  == '!' && buf[len+1] == '#'){
		                    memset(pkt->data,0,MAX_PKT_LEN);
		                    memcpy(pkt->data,buf,len);
							printf("========getpktToSend success========\n");
							printf("      nextNode: %d\n", *nextNode);
							printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
							printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
	  						printf("      header length: %d\n",(pkt->header).length);
	  						printf("      header type: %d\n",(pkt->header).type);
	 					    return 1;
		                }
		                else
		                    return -1;
		            }
		            else
		                return -1;
	            }
				else return -1;
			}
            else
                return -1;
        }
        else
            state = PKTSTART1;
    }
    return -1; 	
}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	char *start = "!&", *end = "!#";
    char *buf;
    buf=(char *)malloc(4+sizeof(sip_hdr_t)+pkt->header.length);
	memcpy(buf,start,2);
	//memcpy(buf+2,&nextNodeID,4);
    memcpy(buf+2,pkt,sizeof(sip_hdr_t)+pkt->header.length);
    memcpy(buf+2+sizeof(sip_hdr_t)+pkt->header.length,end,2);
    if(send(sip_conn, buf, 4+sizeof(sip_hdr_t)+pkt->header.length, 0) < 0){
        printf("========son_sendpkt fail========\n");
        printf("son send pkt error !!!!\n");
        return -1;
	}   
    printf("========forwardpktToSIP success========\n");
    printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
    printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
    printf("      header length: %d\n",(pkt->header).length);
    printf("      header type: %d\n",(pkt->header).type);
    return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	printf("start sendpkt!!\n");
	char *start = "!&", *end = "!#";
    char *buf;
    buf=(char *)malloc(8+sizeof(sip_hdr_t)+pkt->header.length);
	memcpy(buf,start,2);
	int nodeId=0;
	if((pkt->header).type==ROUTE_UPDATE){
		nodeId=BROADCAST_NODEID;
		memcpy(buf+2,&nodeId,4);
	}
    memcpy(buf+6,pkt,sizeof(sip_hdr_t)+pkt->header.length);
    memcpy(buf+6+sizeof(sip_hdr_t)+pkt->header.length,end,2);
    if(send(conn, buf, 8+sizeof(sip_hdr_t)+pkt->header.length, 0) < 0){
        printf("========son_sendpkt fail========\n");
        printf("son send pkt error !!!!\n");
        return -1;
	}   
    printf("========sendpkt success========\n");
	printf("      nodeId: %d\n",nodeId);
    printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
    printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
    printf("      header length: %d\n",(pkt->header).length);
    printf("      header type: %d\n",(pkt->header).type);
    return 1;	
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	printf("start recvpkt!!\n");
	char data[1];
    int n;
    int state = PKTSTART1;
	int nextNode;
    while((n = readn_1(conn,data,1))>0){
        if(data[0] == '!' && state == PKTSTART1){
            state = PKTSTART2;
        }
        else if(data[0] == '&' && state == PKTSTART2){
			if((n = readn_1(conn,(char *)&nextNode,4)) > 0){
		        if((n = readn_1(conn,(char *)&pkt->header,sizeof(sip_hdr_t))) > 0){
		            int len = pkt->header.length;
		            printf("len : %d\n",len);
		            char *buf = (char *)malloc(len+2);
		            memset(buf,0,len+2);
		            if((n = readn_1(conn,buf,len+2))>0){
		                printf("end :   --------   %c %c\n",buf[len],buf[len+1]);
		                if(buf[len]  == '!' && buf[len+1] == '#'){
		                    memset(pkt->data,0,MAX_PKT_LEN);
		                    memcpy(pkt->data,buf,len);
							printf("========recvpkt success========\n");
							printf("      header srcNodeId: %d\n",(pkt->header).src_nodeID);
							printf("      header dextNodeId: %d\n",(pkt->header).dest_nodeID);
	  						printf("      header length: %d\n",(pkt->header).length);
	  						printf("      header type: %d\n",(pkt->header).type);
	 					    return 1;
		                }
		                else
		                    return -1;
		            }
		            else
		                return -1;
	            }
				else return -1;
			}
            else
                return -1;
        }
        else
            state = PKTSTART1;
    }
    return -1; 	
}
