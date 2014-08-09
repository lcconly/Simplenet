//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2013年
#define _BSD_SOURCE
#include "neighbortable.h"
#include <sys/types.h>
#include <sys/socket.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.

nbr_entry_t* nt_create()
{

	printf("创建临界表111\n");
	int nbr_num=topology_getNbrNum();
	printf("邻居结点数目%d\n",nbr_num);
	nbr_entry_t* nbr_table=(nbr_entry_t*)malloc(sizeof(nbr_entry_t)*nbr_num);
	printf("创建临界表222\n");
	int* nbr_array=topology_getNbrArray();
	printf("创建临界表3333\n");
	for(int i=0;i<nbr_num;i++){
		printf("ID  %d\n",nbr_array[i]);
		nbr_table[i].nodeID=nbr_array[i];
		char ip[32];
		topology_getIP(nbr_array[i],ip);
		nbr_table[i].nodeIP=inet_addr(ip);
		nbr_table[i].conn=-1;
	}
	
  return nbr_table;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
	int nbr_num=topology_getNbrNum(); //邻居结点数目
	for(int i=0;i<nbr_num;i++){
		if(nt[i].conn!=-1){
			close(nt[i].conn);
		}
	}
	if(nt!=NULL)
		free(nt);//依次释放内存
	nt=NULL;
  return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	//先检查邻居表中是否有该节点
	int nbr_num=topology_getNbrNum(); //得到邻居结点数目
	int i=0;
	for(;i<nbr_num;i++){
		if(nt[i].nodeID==nodeID){
			nt[i].conn=conn;
			return 1;
		}
	}
	return -1;

}

//根据ID来找到IP
in_addr_t nt_getIP(nbr_entry_t* nt,int size,int nodeID){
	int i=0;
	for(;i<size;i++){
		if(nt[i].nodeID==nodeID)
			return nt[i].nodeIP;
	}
	return 0;
}
