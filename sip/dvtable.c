#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	int nbr_num = topology_getNbrNum();
	int node_num = topology_getNodeNum();
	int *nbr_array = topology_getNbrArray();
	dv_t *dvt = (dv_t*)malloc(sizeof(dv_t) * (nbr_num + 1));
	int i = 0;
	for(; i < nbr_num + 1; i++) {
		if(i == nbr_num)
			dvt[i].nodeID = topology_getMyNodeID();//将自身结点写入表
		else
			dvt[i].nodeID = nbr_array[i];//初始化源结点
		dvt[i].dvEntry = (dv_entry_t *)malloc(sizeof(dv_entry_t) * node_num);
		int *node_array = topology_getNodeArray(), j = 0;
		for(; j < node_num; j++) {
			dvt[i].dvEntry[j].nodeID = node_array[j];//目的结点
			dvt[i].dvEntry[j].cost = topology_getCost(dvt[i].nodeID, node_array[j]);
		}
	}
	return dvt;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	if(dvtable != NULL) {
		if(dvtable->dvEntry != NULL) {
			free(dvtable->dvEntry);
		}
		free(dvtable);
		printf("Destroy successfully\n");
	}

}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i = 0, nbr_num = topology_getNbrNum();
	for(; i < nbr_num + 1; i++) {
		if(dvtable[i].nodeID == fromNodeID) {
			int j = 0, node_num = topology_getNodeNum();
			for(; j < node_num; j++) {
				if(dvtable[i].dvEntry[j].nodeID == toNodeID) {
					dvtable[i].dvEntry[j].cost = cost;
					return 1;
				}
			}
		}
	}
	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i = 0, nbr_num = topology_getNbrNum();
	for(; i < nbr_num + 1; i++) {
		if(dvtable[i].nodeID == fromNodeID) {
			int j = 0, node_num = topology_getNodeNum();
			for(; j < node_num; j++) {
				if(dvtable[i].dvEntry[j].nodeID == toNodeID)
					return dvtable[i].dvEntry[j].cost;
			}
		}
	}
	return INFINITE_COST; 
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	int i = 0, nbr_num = topology_getNbrNum();
	for(; i < nbr_num + 1; i++) {
		int j = 0, node_num = topology_getNodeNum();
		for(; j < node_num; j++) {
			printf("From %d to %d-->Cost:%d\n", dvtable[i].nodeID, dvtable[i].dvEntry[j].nodeID, dvtable[i].dvEntry[j].cost);
		}
	}
	return;
}

/*
int main() {
	printf("Create DvTable\n");
	Dv_Table = dvtable_create();
	dvtable_print(Dv_Table);
	printf("185->186:%d\n", dvtable_getcost(Dv_Table, 185, 186));
	printf("187->186:%d\n", dvtable_getcost(Dv_Table, 187, 186));
	dvtable_setcost(Dv_Table, 187, 186, 20);
	printf("187->186:%d\n", dvtable_getcost(Dv_Table, 187, 186));
	dvtable_print(Dv_Table);
}*/
