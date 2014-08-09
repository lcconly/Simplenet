#include "seg.h"
#include "stdio.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"
#include <errno.h>
#include "pkt.h"
#define SEGSTART1 0
#define SEGSTART2 1


//STCP进程使用这个函数发送sendseg_arg_t结构(包含段及其目的节点ID)给SIP进程.
//参数sip_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t发送成功,就返回1,否则返回-1.
int sip_sendseg(int sip_conn, int dest_nodeID, seg_t* segPtr)
{
//	printf("====sip_sendseg====\n");
	segPtr->header.checksum=checksum(segPtr);
	char *start = "!&", *end = "!#";
    char *buf;
    buf=(char *)malloc(8+sizeof(stcp_hdr_t)+segPtr->header.length);
	memcpy(buf,start,2);
	memcpy(buf+2,&dest_nodeID,4);
    memcpy(buf+6,segPtr,sizeof(stcp_hdr_t)+segPtr->header.length);
    memcpy(buf+6+sizeof(stcp_hdr_t)+segPtr->header.length,end,2);
    if(send(sip_conn, buf, 8+sizeof(stcp_hdr_t)+segPtr->header.length, 0) < 0){
    //   printf("========sip_sendseg fail========\n");
    //    printf("son send pkt error !!!!\n");
        return -1;
	}   
    printf("========sip_sendseg success========\n");
	printf("      destNodeID: %d\n",dest_nodeID);
    printf("      header srcport: %d\n",(segPtr->header).src_port);
    printf("      header dextport: %d\n",(segPtr->header).dest_port);
    printf("      header length: %d\n",(segPtr->header).length);
    printf("      header type: %d\n",(segPtr->header).type);
    return 1;
}

//STCP进程使用这个函数来接收来自SIP进程的包含段及其源节点ID的sendseg_arg_t结构.
//参数sip_conn是STCP进程和SIP进程之间连接的TCP描述符.
//当接收到段时, 使用seglost()来判断该段是否应被丢弃并检查校验和.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
/* readn - read exactly n bytes */
int readn(int fd, char *bp, size_t len)
{
	int cnt;
	int rc;

	cnt = len;
	while (cnt > 0) {
		rc = recv(fd, bp, cnt, 0);
		if (rc < 0) {				/* read error? */
			if (errno == EINTR)	/* interrupted? */
				continue;			/* restart the read */
			return -1;				/* return error */
		}
		if (rc == 0)				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
}


int sip_recvseg(int sip_conn, int* src_nodeID, seg_t* segPtr)
{
	printf("====sip_recvseg====\n");
	char data[1];
    int n;
    int state = SEGSTART1;
    while((n = readn(sip_conn,data,1))>0){
        if(data[0] == '!' && state == SEGSTART1){
            state = SEGSTART2;
        }
        else if(data[0] == '&' && state == SEGSTART2){
			if((n = readn(sip_conn,(char *)src_nodeID,4)) > 0){
                if((n = readn(sip_conn,(char *)&segPtr->header,sizeof(stcp_hdr_t))) > 0){
                    int len = segPtr->header.length;
                    char *buf = (char *)malloc(len+2);
                    memset(buf,0,len+2);
                    if((n = readn(sip_conn,buf,len+2))>0){
                        if(buf[len]  == '!' && buf[len+1] == '#'){
                            memset(segPtr->data,0,MAX_SEG_LEN);
                            memcpy(segPtr->data,buf,len);
                            if(seglost(segPtr) == 0){
                                if(checkchecksum(segPtr) == 1){
              //                      printf("success!!\n");
							        printf("========sip_recvseg success========\n");
							        printf("      srcNodeId: %d\n", *src_nodeID);
							        printf("      header srcport: %d\n",(segPtr->header).src_port);
							        printf("      header dextport: %d\n",(segPtr->header).dest_port);
	  						        printf("      header length: %d\n",(segPtr->header).length);
	  						        printf("      header type: %d\n",(segPtr->header).type);
                                    return 0;
                                }
                                else{
                   //                 printf("=======checksum error=======\n");
                                    return 1;
                                }
                            }
                            else{
                    //            printf("=========seg lost=========\n");
                                return 1;
                            }
                        }
                        else
                            return 2;
                    }
                    else
                        return 2;
                }
                else
                    return 2;
            }
            else 
                return 2;
        }
        else
            state = SEGSTART1;
    }
    return -1; 	
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr)
{
	printf("===getsegToSend===\n");
    char data[1];
    int n;
    int state = SEGSTART1;
    while((n = readn(stcp_conn,data,1))>0){
        if(data[0] == '!' && state == SEGSTART1){
            state = SEGSTART2;
        }
        else if(data[0] == '&' && state == SEGSTART2){
			if((n = readn(stcp_conn,(char *)dest_nodeID,4)) > 0){
                if((n = readn(stcp_conn,(char *)&segPtr->header,sizeof(stcp_hdr_t))) > 0){
                    int len = segPtr->header.length;
                    printf("len : %d\n",len);
                    char *buf = (char *)malloc(len+2);
                    memset(buf,0,len+2);
                    if((n = readn(stcp_conn,buf,len+2))>0){
                        printf("end :   --------   %c %c\n",buf[len],buf[len+1]);
                        if(buf[len]  == '!' && buf[len+1] == '#'){
                            memset(segPtr->data,0,MAX_SEG_LEN);
                            memcpy(segPtr->data,buf,len);
                            //if(seglost(segPtr) == 0){
                            //    if(checkchecksum(segPtr) == 1){
                                    printf("success!!\n");
							        printf("========getsegToSend success========\n");
							        printf("      destNodeId: %d\n", *dest_nodeID);
							        printf("      header srcport: %d\n",(segPtr->header).src_port);
							        printf("      header dextport: %d\n",(segPtr->header).dest_port);
	  						        printf("      header length: %d\n",(segPtr->header).length);
	  						        printf("      header type: %d\n",(segPtr->header).type);
                                    return 0;
                            //    }
                            //    else{
                            //        printf("=======checksum error=======\n");
                            //        return 1;
                            //    }
                            //}
                            //else{
                            //    printf("=========seg lost=========\n");
                            //    return 1;
                            //}
                        }
                        else
                            return 2;
                    }
                    else
                        return 2;
                }
                else
                    return 2;
            }
            else 
                return 2;
        }
        else
            state = SEGSTART1;
    }
    return -1; 
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr)
{
	printf("====forwardsegToSTCP====\n");
	char *start = "!&", *end = "!#";
    char *buf;
    buf=(char *)malloc(8+sizeof(stcp_hdr_t)+segPtr->header.length);
	memcpy(buf,start,2);
	memcpy(buf+2,&src_nodeID,4);
    memcpy(buf+6,segPtr,sizeof(stcp_hdr_t)+segPtr->header.length);
    memcpy(buf+6+sizeof(stcp_hdr_t)+segPtr->header.length,end,2);
    if(send(stcp_conn, buf, 8+sizeof(stcp_hdr_t)+segPtr->header.length, 0) < 0){
        printf("========forwardsegToSTCP fail========\n");
        printf("son send pkt error !!!!\n");
        return -1;
	}   
    printf("========forwardsegToSTCP success========\n");
	printf("      srcNodeID: %d\n",(int)src_nodeID);
    printf("      header srcport: %d\n",(segPtr->header).src_port);
    printf("      header dextport: %d\n",(segPtr->header).dest_port);
    printf("      header length: %d\n",(segPtr->header).length);
    printf("      header type: %d\n",(segPtr->header).type);
    return 1;
}

// 一个段有PKT_LOST_RATE/2的可能性丢失, 或PKT_LOST_RATE/2的可能性有着错误的校验和.
// 如果数据包丢失了, 就返回1, 否则返回0. 
// 即使段没有丢失, 它也有PKT_LOST_RATE/2的可能性有着错误的校验和.
// 我们在段中反转一个随机比特来创建错误的校验和.
int seglost(seg_t* segPtr)
{
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
			return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和计算覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
    unsigned long int cksum=0;
    unsigned short* buf=(unsigned short*)segment ;
    (segment->header).checksum=0;
    int size=sizeof(stcp_hdr_t)+segment->header.length;
    while(size>1){
        cksum += *buf++;
        size-=sizeof(unsigned short int);
    }
    if(size>0)
        cksum+=*(unsigned char*)buf;
    while(cksum>>16)
        cksum=(cksum&0xFFFF)+(cksum>>16);
    return (unsigned short)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1.
int checkchecksum(seg_t* segment)
{
    unsigned long int cksum=0;
    unsigned short* buf=(unsigned short*)segment;
    int size=sizeof(stcp_hdr_t)+segment->header.length;
    while(size>1){
        cksum += *buf++;
        size-=sizeof(unsigned short int);
    }
    if(size>0)
        cksum+=*(unsigned char*)buf;
    while(cksum>>16)
        cksum=(cksum&0xFFFF)+(cksum>>16);
	cksum=(unsigned short)(~cksum);
	//printf("checkchecksum : %d\n",cksum);
    if(cksum==0)
        return 1;
    else return -1;
}
