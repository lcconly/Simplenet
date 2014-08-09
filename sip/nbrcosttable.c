
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"


//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	printf("Create NbrCostTable\n");
	Nbr_Num = topology_getNbrNum();
	nbr_cost_entry_t *nbr_cost_table = (nbr_cost_entry_t *)malloc(sizeof(nbr_cost_entry_t) * Nbr_Num);//创建邻居代价表数组，申请空间
	printf("Nbr_Num:%d\n", Nbr_Num);
	int *nbr_array = topology_getNbrArray();
	int i = 0;
	for(; i < Nbr_Num; i++) {
		nbr_cost_table[i].nodeID = nbr_array[i];
		nbr_cost_table[i].cost = topology_getCost(topology_getMyNodeID(), nbr_array[i]);

	}
	return nbr_cost_table;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	/*int nbr_num=topology_getNbrNum();
	int i;
	for(i=0;i<nbr_num;i++){
		free((nbr_cost_entry_t*)&nct[i]);
	}
	nct=NULL;*/
	if(nct != NULL) 
		free(nct);
//	nct = NULL;
//	if(nct == NULL)
//		printf("nct == NULL\n");
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	if(nct != NULL) {
		int i = 0;
		for(; i < Nbr_Num; i++) {
			if(nodeID == nct[i].nodeID)
				return nct[i].cost;
		}
		return INFINITE_COST;
	}
	else 
		printf("error: nct is NULL\n");
	return -1;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	if(nct != NULL) {
//		printf("nct != NULL\n");
		int i = 0;
		for(; i < Nbr_Num; i++) 
			printf("NodeID:%d, Cost:%d\n", nct[i].nodeID, nct[i].cost);
	}
	else
		printf("error:nct is NULL\n");
}

/*int main() {
	printf("test for nbrcosttable\n");
	printf("Create\n");
	Nbr_Cost_Table = nbrcosttable_create();
	nbrcosttable_print(Nbr_Cost_Table);
	printf("Destroy\n");
	nbrcosttable_destroy(Nbr_Cost_Table);
	if(Nbr_Cost_Table != NULL)
		printf("Nbr_Cost_Table != NULL\n");
	nbrcosttable_print(Nbr_Cost_Table);
	return 0;
}*/
