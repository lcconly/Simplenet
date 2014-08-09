// �ļ��� pkt.c
// ��������: 2013��1��

#include "pkt.h"
#include <string.h>
#include "sys/socket.h"
#include "stdlib.h"
#include "stdio.h"
#include <errno.h>
#include "stdbool.h"
#define PKTSTART1 0
#define PKTSTART2 1
// son_sendpkt()��SIP���̵���, ��������Ҫ��SON���̽����ķ��͵��ص�������. SON���̺�SIP����ͨ��һ������TCP���ӻ���.
// ��son_sendpkt()��, ���ļ�����һ���Ľڵ�ID����װ�����ݽṹsendpkt_arg_t, ��ͨ��TCP���ӷ��͸�SON����. 
// ����son_conn��SIP���̺�SON����֮���TCP�����׽���������.
// ��ͨ��SIP���̺�SON����֮���TCP���ӷ������ݽṹsendpkt_arg_tʱ, ʹ��'!&'��'!#'��Ϊ�ָ���, ����'!& sendpkt_arg_t�ṹ !#'��˳����.
// ������ͳɹ�, ����1, ���򷵻�-1.
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

// son_recvpkt()������SIP���̵���, �������ǽ�������SON���̵ı���. 
// ����son_conn��SIP���̺�SON����֮��TCP���ӵ��׽���������. ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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

// ���������SON���̵���, �������ǽ������ݽṹsendpkt_arg_t.
// ���ĺ���һ���Ľڵ�ID����װ��sendpkt_arg_t�ṹ.
// ����sip_conn����SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// sendpkt_arg_t�ṹͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ɹ�����sendpkt_arg_t�ṹ, ����1, ���򷵻�-1.
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

// forwardpktToSIP()��������SON���̽��յ������ص����������ھӵı��ĺ󱻵��õ�. 
// SON���̵����������������ת����SIP����. 
// ����sip_conn��SIP���̺�SON����֮���TCP���ӵ��׽���������. 
// ����ͨ��SIP���̺�SON����֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// sendpkt()������SON���̵���, �������ǽ�������SIP���̵ı��ķ��͸���һ��.
// ����conn�ǵ���һ���ڵ��TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھӽڵ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#, ����'!& ���� !#'��˳����. 
// ������ķ��ͳɹ�, ����1, ���򷵻�-1.
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

// recvpkt()������SON���̵���, �������ǽ��������ص����������ھӵı���.
// ����conn�ǵ����ھӵ�TCP���ӵ��׽���������.
// ����ͨ��SON���̺����ھ�֮���TCP���ӷ���, ʹ�÷ָ���!&��!#. 
// Ϊ�˽��ձ���, �������ʹ��һ���򵥵�����״̬��FSM
// PKTSTART1 -- ��� 
// PKTSTART2 -- ���յ�'!', �ڴ�'&' 
// PKTRECV -- ���յ�'&', ��ʼ��������
// PKTSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ��� 
// ����ɹ����ձ���, ����1, ���򷵻�-1.
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
