
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{

    return node%MAX_ROUTINGTABLE_SLOTS;


}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{

    routingtable_t* temp_table=(routingtable_t*)malloc(sizeof(routingtable_t));   //动态创建路由表
    int table_index=0;
    for(;table_index<MAX_ROUTINGTABLE_SLOTS;table_index++)
        temp_table->hash[table_index]=NULL;        //初始化为0
    int* nbr_array=topology_getNbrArray();   //得到邻居表
    int nbr_num=topology_getNbrNum();       //得到邻居数
    int index=0;
    int hash_value=0;
    for(;index<nbr_num;index++){
        hash_value=makehash(nbr_array[index]);  //取得在hash槽中的位置
        if(temp_table->hash[hash_value]==NULL){ //第一个插进来的
            temp_table->hash[hash_value]=(routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
            temp_table->hash[hash_value]->destNodeID=nbr_array[index];
            temp_table->hash[hash_value]->nextNodeID=nbr_array[index];
            temp_table->hash[hash_value]->next=NULL;
        }
        else{
            //routingtable_entry_t* ptr=temp_table[]
            routingtable_entry_t* ptr=temp_table->hash[hash_value];
            while(ptr->next!=NULL){
                 ptr=ptr->next;
            }
            //插到槽的最后一个
            ptr->next=(routingtable_entry_t*)malloc(sizeof(routingtable_entry_t
                  ));
            ptr->next->destNodeID=nbr_array[index];
            ptr->next->nextNodeID=nbr_array[index];
            ptr->next->next=NULL;
        }
        
    }
    return  temp_table;

}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	int i;
	for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i++){
		routingtable_entry_t* p = routingtable->hash[i];
		if(p != NULL){
			routingtable_entry_t* q = p;
			free(q);
			p = p->next;
		}
		routingtable->hash[i] = NULL;
	}
  //if(routingtable!=NULL)
	free(routingtable);
	routingtable=NULL;
	return;
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
  int hash_value=makehash(destNodeID);
  routingtable_entry_t* temp=routingtable->hash[hash_value];
  if(temp==NULL)  //新添加路由条目
  {
    routingtable->hash[hash_value]=(routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
    routingtable->hash[hash_value]->destNodeID=destNodeID;
    routingtable->hash[hash_value]->nextNodeID=nextNodeID;
    routingtable->hash[hash_value]->next=NULL;
  //添加新条目结束，直接结束
    return;
  }
  while(temp->next!=NULL){
    //如果已经存在就替换，否则循环结束新添加
    if(temp->destNodeID==destNodeID){
      temp->nextNodeID=nextNodeID;
      break;
    }
    temp=temp->next;
  }
  if(temp->next==NULL)  //如果是最后一个结点
  {
    if(temp->destNodeID==destNodeID){
      temp->nextNodeID=nextNodeID;
      return;
    }
    temp->next=(routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
    temp->nextNodeID=nextNodeID;
    temp->destNodeID=destNodeID;
    return;
  }
  return;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
  int hash_value=makehash(destNodeID);
  routingtable_entry_t* temp=routingtable->hash[hash_value];
  while(temp!=NULL){
    if(temp->destNodeID==destNodeID)
      return temp->nextNodeID;
    temp=temp->next;
  }
  return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
  int i=0;
  routingtable_entry_t* tmp=NULL;
  for(;i<MAX_ROUTINGTABLE_SLOTS;i++)   //依次打印每个槽
  {
    tmp=routingtable->hash[i];
    while(tmp!=NULL)
    {
      printf("Destination node %d\n",tmp->destNodeID);
      printf("       Next node %d\n",tmp->nextNodeID);
      tmp=tmp->next;
    }
  }
  return;
}
