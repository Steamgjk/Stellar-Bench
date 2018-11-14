#ifndef MF_COMMON_H
#define MF_COMMON_H


struct Block
{
	int block_id;
	int data_age;
	int sta_idx;
	int height; //height
	int ele_num;
	bool isP;
	vector<double> eles;
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
struct Updates
{
	int block_id;
	int clock_t;
	int ele_num;
	vector<double> eles;
	Updates()
	{

	}
	Updates operator=(Updates& uitem)
	{
		block_id = uitem.block_id;
		clock_t = uitem.clock_t;
		ele_num = uitem.ele_num;
		eles = uitem.eles;
		return *this;
	}

	void printUpdates()
	{
		printf("update block_id %d\n", block_id );
		printf("clock_t  %d\n", clock_t);
		printf("ele size %ld\n", ele_num);
		for (int i = 0; i < eles.size(); i++)
		{
			printf("%lf\t", eles[i]);
		}
		printf("\n");
	}
};



#endif
