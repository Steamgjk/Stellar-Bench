#ifndef MF_COMMON_H
#define MF_COMMON_H

#define FILE_NAME "./yahoo-output/train-"
#define TEST_NAME "./yahoo-output/test"
#define N 1000990
#define M 624961
#define K  100 //主题个数
#define CAP 2000
#define QP_GROUP 1

struct Block
{
	int block_id;
	int data_age;
	int sta_idx;
	int height; //height
	int ele_num;
	bool isP;
	double* eles;
	Block()
	{

	}
	Block operator=(Block& bitem)
	{
		block_id = bitem.block_id;
		data_age = bitem.data_age;
		height = bitem.height;
		eles = bitem.eles;
		ele_num = bitem.ele_num;
		sta_idx = bitem.sta_idx;
		return *this;
	}
	void printBlock()
	{

		printf("block_id  %d\n", block_id);
		printf("data_age  %d\n", data_age);
		printf("ele_num  %d\n", ele_num);
		for (int i = 0; i < eles.size(); i++)
		{
			printf("%lf\t", eles[i]);
		}
		printf("\n");

	}
};




#endif
