// ÎÄŒþÃû: stcp_server.h
//
// ÃèÊö: ÕâžöÎÄŒþ°üº¬·þÎñÆ÷×ŽÌ¬¶šÒå, Ò»Ð©ÖØÒªµÄÊýŸÝœá¹¹ºÍ·þÎñÆ÷STCPÌ×œÓ×ÖœÓ¿Ú¶šÒå. ÄãÐèÒªÊµÏÖËùÓÐÕâÐ©œÓ¿Ú.
//
// ŽŽœšÈÕÆÚ: 2013Äê1ÔÂ
//

#ifndef STCPSERVER_H
#define STCPSERVER_H

#include <pthread.h>
#include "../common/seg.h"
#include "../common/constants.h"

//FSMÖÐÊ¹ÓÃµÄ·þÎñÆ÷×ŽÌ¬
#define	CLOSED 1
#define	LISTENING 2
#define	CONNECTED 3
#define	CLOSEWAIT 4

typedef struct segBuf {
        seg_t seg;
        unsigned int sentTime;
        struct segBuf* next;
} segBuf_t;


typedef struct server_tcb {
	unsigned int server_nodeID; 
	unsigned int server_portNum;
	unsigned int client_nodeID; 
	unsigned int client_portNum;
	unsigned int state;         
	unsigned int expect_seqNum;     
	char* recvBuf;              
	unsigned int  usedBufLen;   
	pthread_mutex_t* bufMutex_recv;      
	//服务器的发送缓冲区一些东西	
	unsigned int next_seqNum;       //下一个序号
	pthread_mutex_t* bufMutex_send;	//互斥量用于控制服务器发送缓冲区的访问
	segBuf_t* sendBufHead;		//发送缓冲区头
	segBuf_t* sendBufunSent;         //发送缓冲区第一个未发送段
	segBuf_t* sendBufTail;		//发送缓冲区尾
	unsigned int unAck_segNum;      //已发送但未收到确认段的数量
} server_tcb_t;

//
//  ÓÃÓÚ·þÎñÆ÷¶ËÓŠÓÃ³ÌÐòµÄSTCPÌ×œÓ×ÖAPI. 
//  ===================================
//
//  ÎÒÃÇÔÚÏÂÃæÌá¹©ÁËÃ¿žöº¯Êýµ÷ÓÃµÄÔ­ÐÍ¶šÒåºÍÏžœÚËµÃ÷, µ«ÕâÐ©Ö»ÊÇÖžµŒÐÔµÄ, ÄãÍêÈ«¿ÉÒÔžùŸÝ×ÔŒºµÄÏë·šÀŽÉèŒÆŽúÂë.
//
//  ×¢Òâ: µ±ÊµÏÖÕâÐ©º¯ÊýÊ±, ÄãÐèÒª¿ŒÂÇFSMÖÐËùÓÐ¿ÉÄÜµÄ×ŽÌ¬, Õâ¿ÉÒÔÊ¹ÓÃswitchÓïŸäÀŽÊµÏÖ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_server_init(int conn);

// Õâžöº¯Êý³õÊŒ»¯TCB±í, œ«ËùÓÐÌõÄ¿±êŒÇÎªNULL. Ëü»¹Õë¶ÔTCPÌ×œÓ×ÖÃèÊö·ûconn³õÊŒ»¯Ò»žöSTCP²ãµÄÈ«ŸÖ±äÁ¿, 
// žÃ±äÁ¿×÷Îªsip_sendsegºÍsip_recvsegµÄÊäÈë²ÎÊý. ×îºó, Õâžöº¯ÊýÆô¶¯seghandlerÏß³ÌÀŽŽŠÀíœøÈëµÄSTCP¶Î.
// ·þÎñÆ÷Ö»ÓÐÒ»žöseghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_sock(unsigned int server_port);

// Õâžöº¯Êý²éÕÒ·þÎñÆ÷TCB±íÒÔÕÒµœµÚÒ»žöNULLÌõÄ¿, È»ºóÊ¹ÓÃmalloc()ÎªžÃÌõÄ¿ŽŽœšÒ»žöÐÂµÄTCBÌõÄ¿.
// žÃTCBÖÐµÄËùÓÐ×Ö¶Î¶Œ±»³õÊŒ»¯, ÀýÈç, TCB state±»ÉèÖÃÎªCLOSED, ·þÎñÆ÷¶Ë¿Ú±»ÉèÖÃÎªº¯Êýµ÷ÓÃ²ÎÊýserver_port. 
// TCB±íÖÐÌõÄ¿µÄË÷ÒýÓŠ×÷Îª·þÎñÆ÷µÄÐÂÌ×œÓ×ÖID±»Õâžöº¯Êý·µ»Ø, ËüÓÃÓÚ±êÊ¶·þÎñÆ÷¶ËµÄÁ¬œÓ. 
// Èç¹ûTCB±íÖÐÃ»ÓÐÌõÄ¿¿ÉÓÃ, Õâžöº¯Êý·µ»Ø-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_accept(int sockfd);





//服务器发送数据给客户端，。这个函数用套接字ID找到TCB表中的条目
//参考网络实验教材P74
int stcp_server_send(int sockfd, void* data, unsigned int length);

// Õâžöº¯ÊýÊ¹ÓÃsockfd»ñµÃTCBÖžÕë, ²¢œ«Á¬œÓµÄstate×ª»»ÎªLISTENING. ËüÈ»ºóÆô¶¯¶šÊ±Æ÷œøÈëÃŠµÈŽýÖ±µœTCB×ŽÌ¬×ª»»ÎªCONNECTED 
// (µ±ÊÕµœSYNÊ±, seghandler»áœøÐÐ×ŽÌ¬µÄ×ª»»). žÃº¯ÊýÔÚÒ»žöÎÞÇîÑ­»·ÖÐµÈŽýTCBµÄstate×ª»»ÎªCONNECTED,  
// µ±·¢ÉúÁË×ª»»Ê±, žÃº¯Êý·µ»Ø1. Äã¿ÉÒÔÊ¹ÓÃ²»Í¬µÄ·œ·šÀŽÊµÏÖÕâÖÖ×èÈûµÈŽý.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_recv(int sockfd, void* buf, unsigned int length);

// œÓÊÕÀŽ×ÔSTCP¿Í»§¶ËµÄÊýŸÝ. Çë»ØÒäSTCPÊ¹ÓÃµÄÊÇµ¥ÏòŽ«Êä, ÊýŸÝŽÓ¿Í»§¶Ë·¢ËÍµœ·þÎñÆ÷¶Ë.
// ÐÅºÅ/¿ØÖÆÐÅÏ¢(ÈçSYN, SYNACKµÈ)ÔòÊÇË«ÏòŽ«µÝ. Õâžöº¯ÊýÃ¿žôRECVBUF_POLLING_INTERVALÊ±Œä
// ŸÍ²éÑ¯œÓÊÕ»º³åÇø, Ö±µœµÈŽýµÄÊýŸÝµœŽï, ËüÈ»ºóŽæŽ¢ÊýŸÝ²¢·µ»Ø1. Èç¹ûÕâžöº¯ÊýÊ§°Ü, Ôò·µ»Ø-1.
//
// ×¢Òâ: stcp_server_recvÔÚ·µ»ØÊýŸÝžøÓŠÓÃ³ÌÐòÖ®Ç°, Ëü×èÈûµÈŽýÓÃ»§ÇëÇóµÄ×ÖœÚÊý(ŒŽlength)µœŽï·þÎñÆ÷.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_server_close(int sockfd);

// Õâžöº¯Êýµ÷ÓÃfree()ÊÍ·ÅTCBÌõÄ¿. Ëüœ«žÃÌõÄ¿±êŒÇÎªNULL, ³É¹ŠÊ±(ŒŽÎ»ÓÚÕýÈ·µÄ×ŽÌ¬)·µ»Ø1,
// Ê§°ÜÊ±(ŒŽÎ»ÓÚŽíÎóµÄ×ŽÌ¬)·µ»Ø-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void* seghandler(void* arg);

// ÕâÊÇÓÉstcp_server_init()Æô¶¯µÄÏß³Ì. ËüŽŠÀíËùÓÐÀŽ×Ô¿Í»§¶ËµÄœøÈëÊýŸÝ. seghandler±»ÉèŒÆÎªÒ»žöµ÷ÓÃsip_recvseg()µÄÎÞÇîÑ­»·, 
// Èç¹ûsip_recvseg()Ê§°Ü, ÔòËµÃ÷µœSIPœø³ÌµÄÁ¬œÓÒÑ¹Ø±Õ, Ïß³Ìœ«ÖÕÖ¹. žùŸÝSTCP¶ÎµœŽïÊ±Á¬œÓËùŽŠµÄ×ŽÌ¬, ¿ÉÒÔ²ÉÈ¡²»Í¬µÄ¶¯×÷.
// Çë²é¿Ž·þÎñ¶ËFSMÒÔÁËœâžü¶àÏžœÚ.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

//当发送缓冲区非空时，该线程运行，每隔一段时间就去查询第一个已发送但是未被确认段，如果超时(当前时间和段中有记录时间之差大于间隔)，
//发送缓冲区中所有已发送但未被确认的数据段都会被重发
//当函数stcp_client_send()发现当前发送缓冲区为空，就会启动该线程，而当发送缓冲区所有数据段都被确认了，该线程自己终止执行
void* sendBuf_timer(void* clienttcb);

#endif
