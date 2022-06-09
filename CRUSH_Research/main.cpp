#include <iostream>
#include <vector>
#include <pthread.h>
#include <chrono>
#include <unistd.h>
#include <algorithm>
#include "include/rseudo_random_number.h"
#include "include/threadpool.h"

using namespace std;
#define TYPE 0    //rjenkins type
#define BUCKETS 11
int osd_sum = 10000;
int host_osd_nums = 15;
int global_osd_id = 0;
int global_bucket_id = 0;
__u32 *weight = new __u32[osd_sum];
__u32 *reweight = new __u32[osd_sum];
int weight_max = osd_sum;
pthread_mutex_t mutex;
int pthread_outpos = 0;
int object_num;


crush_bucket_straw2* construct_host(struct crush_bucket_straw2 **buckets, int id)    //当前bucket的id
{
	crush_bucket_straw2* host = new crush_bucket_straw2();
	host->h.id = id;
	host->h.alg = 6;
	host->h.hash = 0;
	host->h.type = 1;
	host->h.size = host_osd_nums;
	__u32 *item_weights = new __u32[host_osd_nums];
	host->h.weight = 0;
	int *items = new int[host_osd_nums];
	for(int j = 0;j < host->h.size;++j)
	{
		int cur_osd_id = global_osd_id++;
		items[j] = cur_osd_id;
		item_weights[j] = weight[cur_osd_id];
		host->h.weight += weight[cur_osd_id];
	}
	host->h.items = items;
	host->item_weights = item_weights;
	return host;
}

crush_bucket_straw2* construct_rack(struct crush_bucket_straw2 **buckets, int id)    //当前bucket的id
{
	crush_bucket_straw2* rack = new crush_bucket_straw2();
	rack->h.id = id;
	rack->h.alg = 6;
	rack->h.hash = 0;
	rack->h.type = 3;
	rack->h.size = 10;
	__u32 *item_weights = new __u32[rack->h.size];
	int *items = new int[rack->h.size];
	rack->h.weight = 0;
	for(int j = 0;j < rack->h.size;++j)
	{
		int item_id = global_bucket_id++;
		buckets[item_id] = construct_host(buckets,-item_id-1);
		items[j] = -item_id-1;
		item_weights[j] = buckets[item_id]->h.weight;
		rack->h.weight += buckets[item_id]->h.weight;
	}
	rack->h.items = items;
	rack->item_weights = item_weights;
	return rack;
}

crush_bucket_straw2* construct_root(struct crush_bucket_straw2 **buckets)
{
	crush_bucket_straw2* root = new crush_bucket_straw2();
	buckets[global_bucket_id++] = root;
	root->h.id = -1;
	root->h.alg = 6;
	root->h.hash = 0;
	root->h.type = 10;
	root->h.size = 5;
	__u32 *item_weights = new __u32[root->h.size];
	int *items = new int[root->h.size];
	root->h.weight = 0;
	for(int j = 0;j < root->h.size;++j)
	{
		int item_id = global_bucket_id++;
		buckets[item_id] = construct_rack(buckets,-item_id-1);
		items[j] = -item_id-1;
		item_weights[j] = buckets[item_id]->h.weight;
		root->h.weight += buckets[item_id]->h.weight;
	}
	root->h.items = items;
	root->item_weights = item_weights;
	return root;
}

//初始化cluster map，其中root下有5个rack，每个rack下有10台host，每台host下有12个OSD。
void init2(struct crush_map *map,struct crush_bucket_straw2 **buckets)
{
	for(int i = 0;i < osd_sum; ++i)
	{
		weight[i] = 176;
		reweight[i] = 65536;
	}
	construct_root(buckets);
	map->buckets = (struct crush_bucket**)buckets;
    //先不考虑构造rule
    struct crush_rule **rules = NULL;   
    map->rules = rules;

    map->max_buckets = global_bucket_id;
    map->max_rules = 2;
    map->max_devices = global_osd_id;
    map->choose_local_tries = 0;
    map->straw_calc_version = 1;
    map->choose_total_tries = 50;
    map->choose_local_fallback_tries = 0;
    map->chooseleaf_descend_once = 1;
    map->chooseleaf_vary_r = 1;
    map->chooseleaf_stable = 1;
    map->allowed_bucket_algs = 54; 
}


//构造crush_map，-1为root节点，下面是10个host节点，host节点有10个osd。
void init(struct crush_map *map,struct crush_bucket_straw2 **buckets,__u32 *weight,int nums)
{
    //构造bucket，-1为root节点，下面是10个host节点，host节点有10个osd。
	for(int i = 0;i < BUCKETS;++i)
	{
		buckets[i] = new crush_bucket_straw2();
	}
    
    //host
    int osd = 0;
    for(int i = 1;i < BUCKETS; ++i)
    {
        buckets[i]->h.id = -1-i;
        buckets[i]->h.alg = 6;
        buckets[i]->h.hash = 0;
        buckets[i]->h.type = 1;
        buckets[i]->h.size = nums/10;
        int *items = new int[nums/10];
		__u32 *item_weights = new __u32[nums/10];
		buckets[i]->h.weight = 0;
        for(int j = 0;j < buckets[i]->h.size;++j)
        {
			item_weights[j] = weight[osd];
			buckets[i]->h.weight += weight[osd];
            items[j] = osd++;
        }
        buckets[i]->h.items = items;
		buckets[i]->item_weights = item_weights;
    }
	//root
    buckets[0]->h.id = -1;
    buckets[0]->h.alg = 6;
    buckets[0]->h.hash = 0;
    buckets[0]->h.type = 11;
    buckets[0]->h.size = 10;
    int *items = new int[10]{-2,-3,-4,-5,-6,-7,-8,-9,-10,-11};
	__u32 *item_weights = new __u32[10];
	for(int j = 0;j < buckets[0]->h.size;++j)
	{
		item_weights[j] = buckets[-1-items[j]]->h.weight;
		buckets[0]->h.weight += buckets[-1-items[j]]->h.weight;
	}
	buckets[0]->h.items = items;
	buckets[0]->item_weights = item_weights;
    map->buckets = (struct crush_bucket**)buckets;

    //先不考虑构造rule
    struct crush_rule **rules = NULL;   
    map->rules = rules;

    map->max_buckets = 11;
    map->max_rules = 2;
    map->max_devices = osd-1;
    map->choose_local_tries = 0;
    map->straw_calc_version = 1;
    map->choose_total_tries = 50;
    map->choose_local_fallback_tries = 0;
    map->chooseleaf_descend_once = 1;
    map->chooseleaf_vary_r = 1;
    map->chooseleaf_stable = 1;
    map->allowed_bucket_algs = 54; 
}

static inline __s32 *get_choose_arg_ids(const struct crush_bucket_straw2 *bucket,
					                    const struct crush_choose_arg *arg)
{
	if ((arg == NULL) || (arg->ids == NULL))
		return bucket->h.items;
	return arg->ids;
}

static inline __u32 *get_choose_arg_weights(const struct crush_bucket_straw2 *bucket,
                                            const struct crush_choose_arg *arg,
                                            int position)
{
	if ((arg == NULL) || (arg->weight_set == NULL))
		return bucket->item_weights;
	if (position >= arg->weight_set_positions)
		position = arg->weight_set_positions - 1;
	return arg->weight_set[position].weights;
}

static int bucket_straw2_choose(const struct crush_bucket_straw2 *bucket,
				                int x, 
                                int r, 
                                const struct crush_choose_arg *arg,
                                int position)   //x: pgid    y: 副本
{
   unsigned int i, high = 0;
	__s64 draw, high_draw = 0;
    __u32 *weights = get_choose_arg_weights(bucket, arg, position);    //weights：bucket的item的权重数组
    __s32 *ids = get_choose_arg_ids(bucket, arg);   //ids: bucket的itemID数组
	//cout<<bucket->h.size<<endl;
	for (i = 0; i < bucket->h.size; i++) {
		if (weights[i]) {
			//将pg的ID:x、bucket的itemID、随机因子r、bucket的item的权重计算随机数
			draw = generate_exponential_distribution(bucket->h.hash, x, ids[i], r, weights[i]);  
		} else {
			draw = S64_MIN;
		}

		if (i == 0 || draw > high_draw) {
			high = i;
			high_draw = draw;
		}
	}
	return bucket->h.items[high];
}


static int crush_bucket_choose(const struct crush_bucket *in,
			                    struct crush_work_bucket *work,
			                    int x, 
                                int r,
                                const struct crush_choose_arg *arg,
                                int position)
{
	return bucket_straw2_choose((const struct crush_bucket_straw2 *)in, x, r, arg, position);
}

static int is_out(const struct crush_map *map,
		  const __u32 *weight, int weight_max,
		  int item, int x)
{
	if (item >= weight_max)
		return 1;
	if (weight[item] >= 0x10000)
		return 0;
	if (weight[item] == 0)
		return 1;
	if ((crush_hash32_2(CRUSH_HASH_RJENKINS1, x, item) & 0xffff) < weight[item])
		return 0;
	return 1;
}


//crush算法
static int crush_choose_firstn(const struct crush_map *map,
                        struct crush_work *work,
                        const struct crush_bucket *bucket,
                        const __u32 *weight, 
                        int weight_max,
                        int x, 
                        int numrep, 
                        int type,
                        int *out, 
                        int outpos,
                        int out_size,
                        unsigned int tries,
                        unsigned int recurse_tries,
                        unsigned int local_retries,
                        unsigned int local_fallback_retries,
                        int recurse_to_leaf,
                        unsigned int vary_r,
                        unsigned int stable,
                        int *out2,
                        int parent_r,
                        const struct crush_choose_arg *choose_args)
{
    //cout<<x<<endl;
    int rep;    //计数器
	unsigned int ftotal, flocal;
	int retry_descent, retry_bucket, skip_rep;
	const struct crush_bucket *in = bucket;
	int r;
	int i;
	int item = 0;   //bucket的序号
	int itemtype;
	int collide, reject;
	int count = out_size;
    for (rep = stable ? 0 : outpos; rep < numrep && count > 0 ; rep++)    //PG  循环3次
    {
        ftotal = 0;
		skip_rep = 0;
        do {                                    //每次计算一个新的OSD，当前计算OSD失败或冲突时的循环,随机因子r改变
			retry_descent = 0;              //重置尝试次数
			in = bucket;              /* initial bucket */  //每次计算一个新的OSD时，设置当前开始查找的bucket为函数输入的bucket
			// if(in->id < 0)
			// 	printf("in.id = %d, in.type = %d, in.size = %d\n",in->id,in->type,in->size);
			/* choose through intervening buckets */
			flocal = 0;
			do {                              //当前item type不是OSD时的循环，当前进行选择的bucket，即in改变。
				collide = 0;
				retry_bucket = 0;
				r = rep + parent_r;           //设置随机因子r
				/* r' = r + f_total */
				//cout<<pthread_self()<<" "<<r<<" "<<ftotal<<endl;
				r += ftotal;
				/* bucket choose */
				if (in->size == 0) {
					reject = 1;
					goto reject;
				}
				
				//在当前开始查找的in下选择一个item
				item = crush_bucket_choose(in, nullptr, x, r, (choose_args ? &choose_args[-1-in->id] : 0),outpos);
				//cout<<item<<endl;
				if (item >= map->max_devices) {      //map->max_devices是最大的数
					skip_rep = 1;
					break;
				}

				/* desired type? */    //获取item类型
				if (item < 0)       //非osd的节点item都小于0
					itemtype = map->buckets[-1-item]->type;    //通过-1-item可以获取当前bucket的子bucket的类型
				else
					itemtype = 0;    //osd的id都大于等于0
				/* keep going? */   //判断计算得到的item是否输入的type
				if (itemtype != type) {
					if (item >= 0 ||(-1-item) >= map->max_buckets) 
					{
						skip_rep = 1;
						break;
					}
					in = map->buckets[-1-item];     //计算得到的item不是输入的type，获取当前bucket的item结构
					retry_bucket = 1;
					//此时的item必不是osd
					//printf("item = %d\n", item);
					continue;    //继续循环，找到osd为止
				}
				//printf("item = %d\n", item);
				//此时的item必是osd
				/* collision? */    //item是否与osd[]里面的osd相同？
				// cout<<pthread_self()<<" "<<outpos<<" "<<item<<endl;
				for (i = 0; i < outpos; i++) {
					if (out[i] == item) {
						collide = 1;
						break;
					}
				}

				reject = 0;
				if (!collide && recurse_to_leaf) {    //无冲突，进入判断是否在故障域内
					if (item < 0) {
						int sub_r;
						if (vary_r)
							sub_r = r >> (vary_r-1);
						else
							sub_r = 0;
						if (crush_choose_firstn(          //容灾域故障，重置期望类型为0，直至找到osd,递归调用crush_choose_firstn算法
							    map,
							    work,
							    map->buckets[-1-item],
							    weight, 
                                weight_max,
							    x, 
                                stable ? 1 : outpos+1, 
                                0,
							    out2, 
                                outpos, 
                                count,
							    recurse_tries, 
                                0,
							    local_retries,
							    local_fallback_retries,
							    0,
							    vary_r,
							    stable,
							    NULL,
							    sub_r,
                                choose_args) <= outpos)
							/* didn't get leaf */
							reject = 1;
					} else {                          //无容灾域故障，将item放入输出out2数组中
					/* we already have a leaf! */
						out2[outpos] = item;
					}
				}
				// cout<<map<<" "<<weight<<" "<<weight_max<<" "<<item<<endl;
				if (!reject && !collide) {    //无冲突，进入判断osd是否过载
					/* out? */
					if (itemtype == 0)   //type 0 osd
					{
						//cout<<map<<" "<<weight<<" "<<weight_max<<" "<<item<<" "<<x<<endl;
						reject = is_out(map, weight,weight_max,item, x);
					}
						
				}

reject:
				if (reject || collide) {
					ftotal++;
					flocal++;

					if (collide && flocal <= local_retries)
						/* retry locally a few times */
						retry_bucket = 1;
					else if (local_fallback_retries > 0 &&
						 flocal <= in->size + local_fallback_retries)
						/* exhaustive bucket search */
						retry_bucket = 1;
					else if (ftotal < tries)
					{
						/* then retry descent */
						retry_descent = 1;
					}
					else
						/* else give up */
						skip_rep = 1;
				}
			} while (retry_bucket);
		} while (retry_descent);
		if (skip_rep) {
			continue;
		}
		//item
		out[outpos] = item;
		outpos++;
		count--;
    }
	return outpos;
}

struct firstn_info
{
	const struct crush_map *map;
	struct crush_work *work;
	const struct crush_bucket *bucket;
	const __u32 *weight;
	int weight_max;
	int x;
	int numrep; 
	int type;
	int *out;
	int outpos;
	int out_size;
	unsigned int tries;
	unsigned int recurse_tries;
	unsigned int local_retries;
	unsigned int local_fallback_retries;
	int recurse_to_leaf;
	unsigned int vary_r;
	unsigned int stable;
	int *out2;
	int *parent_r;
	const struct crush_choose_arg *choose_args;
};


void* do_work(const void *arg)
{
	struct firstn_info* firstn = (struct firstn_info*)arg;
	cout<<pthread_self()<<" "<<firstn->parent_r<<endl;
	int parent_r = *firstn->parent_r;
	//cout<<pthread_self()<<" "<<parent_r<<endl;
	int rep;    //计数器
	unsigned int ftotal, flocal;
	int retry_descent, retry_bucket, skip_rep;
	const struct crush_bucket *in = firstn->bucket;
	int r;
	int i;
	int item = 0;   //bucket的序号
	int itemtype;
	int collide, reject;
	int count = firstn->out_size;
	pthread_mutex_lock(&mutex);
	rep = firstn->stable ? 0 : pthread_outpos;
	pthread_mutex_unlock(&mutex);
	int testnum = 0;
	ftotal = 0;
	skip_rep = 0;
	do {                                    //每次计算一个新的OSD，当前计算OSD失败或冲突时的循环,随机因子r改变
		retry_descent = 0;              //重置尝试次数
		in = firstn->bucket;              /* initial bucket */  //每次计算一个新的OSD时，设置当前开始查找的bucket为函数输入的bucket
		// if(in->id < 0)
		/* choose through intervening buckets */
		flocal = 0;
		do {                              //当前item type不是OSD时的循环，当前进行选择的bucket，即in改变。
			collide = 0;
			retry_bucket = 0;
			r = rep + parent_r;           //设置随机因子r
			/* r' = r + f_total */
			//cout<<pthread_self()<<" "<<r<<" "<<ftotal<<endl;
			r += ftotal;
			
			/* bucket choose */
			if (in->size == 0) {
				reject = 1;
				goto reject;
			}
			
			//在当前开始查找的in下选择一个item
			item = crush_bucket_choose(in, nullptr, firstn->x, r, (firstn->choose_args ? &firstn->choose_args[-1-in->id] : 0),firstn->outpos);
			//cout<<item<<endl;
			// cout<<item<<endl;
			if (item >= firstn->map->max_devices) {      //map->max_devices是最大的数
				skip_rep = 1;
				break;
			}

			/* desired type? */    //获取item类型
			if (item < 0)       //非osd的节点item都小于0
				itemtype = firstn->map->buckets[-1-item]->type;    //通过-1-item可以获取当前bucket的子bucket的类型
			else
				itemtype = 0;    //osd的id都大于等于0
			/* keep going? */   //判断计算得到的item是否输入的type
			if (itemtype != firstn->type) {
				if (item >= 0 ||(-1-item) >= firstn->map->max_buckets) 
				{
					skip_rep = 1;
					break;
				}
				in = firstn->map->buckets[-1-item];     //计算得到的item不是输入的type，获取当前bucket的item结构
				retry_bucket = 1;
				//此时的item必不是osd
				//printf("item = %d\n", item);
				continue;    //继续循环，找到osd为止
			}
			//printf("item = %d\n", item);
			//此时的item必是osd
			/* collision? */    //item是否与osd[]里面的osd相同？
			pthread_mutex_lock(&mutex);
			// cout<<pthread_self()<<" "<<pthread_outpos<<" "<<item<<endl;
			for (i = 0; i < pthread_outpos; i++) {
				//cout<<i<<" "<<firstn->out[i]<<endl;
				if (firstn->out[i] == item) {
					collide = 1;
					break;
				}
			}
			pthread_mutex_unlock(&mutex);

			reject = 0;
			if (!collide && firstn->recurse_to_leaf) {    //无冲突，进入判断是否在故障域内
				if (item < 0) {
					int sub_r;
					if (firstn->vary_r)
						sub_r = r >> (firstn->vary_r-1);
					else
						sub_r = 0;
					if (crush_choose_firstn(          //容灾域故障，重置期望类型为0，直至找到osd,递归调用crush_choose_firstn算法
							firstn->map,
							firstn->work,
							firstn->map->buckets[-1-item],
							weight, 
							weight_max,
							firstn->x, 
							firstn->stable ? 1 : firstn->outpos+1, 
							0,
							firstn->out2, 
							firstn->outpos, 
							count,
							firstn->recurse_tries, 
							0,
							firstn->local_retries,
							firstn->local_fallback_retries,
							0,
							firstn->vary_r,
							firstn->stable,
							NULL,
							sub_r,
							firstn->choose_args) <= firstn->outpos)
						/* didn't get leaf */
						reject = 1;
				} else {                          //无容灾域故障，将item放入输出out2数组中
				/* we already have a leaf! */
					pthread_mutex_lock(&mutex);
					firstn->out2[pthread_outpos] = item;
					pthread_mutex_unlock(&mutex);
				}
			}
			if (!reject && !collide) {    //无冲突，进入判断osd是否过载
				/* out? */
				if (itemtype == 0)   //type 0 osd
				{
					//cout<<firstn->map<<" "<<firstn->weight<<" "<<weight_max<<" "<<item<<" "<<firstn->x<<endl;
					reject = is_out(firstn->map, firstn->weight,weight_max,item, firstn->x);
				}
			}
reject:
			if (reject || collide) {
				ftotal++;
				flocal++;

				if (collide && flocal <= firstn->local_retries)
					/* retry locally a few times */
					retry_bucket = 1;
				else if (firstn->local_fallback_retries > 0 &&
						flocal <= in->size + firstn->local_fallback_retries)
					/* exhaustive bucket search */
					retry_bucket = 1;
				else if (ftotal < firstn->tries)
				{
					/* then retry descent */
					retry_descent = 1;
				}
				else
					/* else give up */
					skip_rep = 1;
			}
		} while (retry_bucket);
	} while (retry_descent);
	pthread_mutex_lock(&mutex);
	firstn->out[pthread_outpos] = item;
	pthread_outpos++;
	pthread_mutex_unlock(&mutex);
}

void* do_work1(void *arg)
{
	struct firstn_info* firstn = (struct firstn_info*)arg;
	//cout<<pthread_self()<<" "<<firstn->parent_r<<endl;
	int parent_r = 0;
	int outpos = 0;
	//cout<<pthread_self()<<" "<<firstn->x<<endl;
	int rep;    //计数器
	unsigned int ftotal, flocal;
	int retry_descent, retry_bucket, skip_rep;
	const struct crush_bucket *in = firstn->bucket;
	int r;
	int i;
	int item = 0;   //bucket的序号
	int itemtype;
	int collide, reject;
	int count = firstn->out_size;
	pthread_mutex_lock(&mutex);
	rep = firstn->stable ? 0 : outpos;
	pthread_mutex_unlock(&mutex);
	int testnum = 0;
	ftotal = 0;
	skip_rep = 0;
	do {                                    //每次计算一个新的OSD，当前计算OSD失败或冲突时的循环,随机因子r改变
		retry_descent = 0;              //重置尝试次数
		in = firstn->bucket;              /* initial bucket */  //每次计算一个新的OSD时，设置当前开始查找的bucket为函数输入的bucket
		// if(in->id < 0)
		/* choose through intervening buckets */
		flocal = 0;
		do {                              //当前item type不是OSD时的循环，当前进行选择的bucket，即in改变。
			collide = 0;
			retry_bucket = 0;
			r = rep + parent_r;           //设置随机因子r
			/* r' = r + f_total */
			//cout<<pthread_self()<<" "<<r<<" "<<ftotal<<endl;
			r += ftotal;
			
			/* bucket choose */
			if (in->size == 0) {
				reject = 1;
				goto reject;
			}
			
			//在当前开始查找的in下选择一个item
			item = crush_bucket_choose(in, nullptr, firstn->x, r, (firstn->choose_args ? &firstn->choose_args[-1-in->id] : 0),firstn->outpos);
			//cout<<item<<endl;
			// cout<<item<<endl;
			if (item >= firstn->map->max_devices) {      //map->max_devices是最大的数
				skip_rep = 1;
				break;
			}

			/* desired type? */    //获取item类型
			if (item < 0)       //非osd的节点item都小于0
				itemtype = firstn->map->buckets[-1-item]->type;    //通过-1-item可以获取当前bucket的子bucket的类型
			else
				itemtype = 0;    //osd的id都大于等于0
			/* keep going? */   //判断计算得到的item是否输入的type
			if (itemtype != firstn->type) {
				if (item >= 0 ||(-1-item) >= firstn->map->max_buckets) 
				{
					skip_rep = 1;
					break;
				}
				in = firstn->map->buckets[-1-item];     //计算得到的item不是输入的type，获取当前bucket的item结构
				retry_bucket = 1;
				//此时的item必不是osd
				//printf("item = %d\n", item);
				continue;    //继续循环，找到osd为止
			}
			//printf("item = %d\n", item);
			//此时的item必是osd
			/* collision? */    //item是否与osd[]里面的osd相同？
			pthread_mutex_lock(&mutex);
			// cout<<pthread_self()<<" "<<pthread_outpos<<" "<<item<<endl;
			for (i = 0; i < outpos; i++) {
				//cout<<i<<" "<<firstn->out[i]<<endl;
				if (firstn->out[i] == item) {
					collide = 1;
					break;
				}
			}
			pthread_mutex_unlock(&mutex);

			reject = 0;
			if (!collide && firstn->recurse_to_leaf) {    //无冲突，进入判断是否在故障域内
				if (item < 0) {
					int sub_r;
					if (firstn->vary_r)
						sub_r = r >> (firstn->vary_r-1);
					else
						sub_r = 0;
					if (crush_choose_firstn(          //容灾域故障，重置期望类型为0，直至找到osd,递归调用crush_choose_firstn算法
							firstn->map,
							firstn->work,
							firstn->map->buckets[-1-item],
							weight, 
							weight_max,
							firstn->x, 
							firstn->stable ? 1 : firstn->outpos+1, 
							0,
							firstn->out2, 
							firstn->outpos, 
							count,
							firstn->recurse_tries, 
							0,
							firstn->local_retries,
							firstn->local_fallback_retries,
							0,
							firstn->vary_r,
							firstn->stable,
							NULL,
							sub_r,
							firstn->choose_args) <= firstn->outpos)
						/* didn't get leaf */
						reject = 1;
				} else {                          //无容灾域故障，将item放入输出out2数组中
				/* we already have a leaf! */
					pthread_mutex_lock(&mutex);
					firstn->out2[outpos] = item;
					pthread_mutex_unlock(&mutex);
				}
			}
			if (!reject && !collide) {    //无冲突，进入判断osd是否过载
				/* out? */
				if (itemtype == 0)   //type 0 osd
				{
					//cout<<firstn->map<<" "<<firstn->weight<<" "<<weight_max<<" "<<item<<" "<<firstn->x<<endl;
					reject = is_out(firstn->map, firstn->weight,weight_max,item, firstn->x);
				}
			}
reject:
			if (reject || collide) {
				ftotal++;
				flocal++;

				if (collide && flocal <= firstn->local_retries)
					/* retry locally a few times */
					retry_bucket = 1;
				else if (firstn->local_fallback_retries > 0 &&
						flocal <= in->size + firstn->local_fallback_retries)
					/* exhaustive bucket search */
					retry_bucket = 1;
				else if (ftotal < firstn->tries)
				{
					/* then retry descent */
					retry_descent = 1;
				}
				else
					/* else give up */
					skip_rep = 1;
			}
		} while (retry_bucket);
	} while (retry_descent);
	pthread_mutex_lock(&mutex);
	firstn->out[outpos] = item;
	firstn->outpos++;
	pthread_mutex_unlock(&mutex);
	//pthread_exit(NULL);
}

void* do_work2(void *arg)
{
	struct firstn_info* firstn = (struct firstn_info*)arg;
	//cout<<pthread_self()<<" "<<firstn->parent_r<<endl;
	int parent_r = 1;
	int outpos = 1;
	//cout<<pthread_self()<<" "<<firstn->x<<endl;
	int rep;    //计数器
	unsigned int ftotal, flocal;
	int retry_descent, retry_bucket, skip_rep;
	const struct crush_bucket *in = firstn->bucket;
	int r;
	int i;
	int item = 0;   //bucket的序号
	int itemtype;
	int collide, reject;
	int count = firstn->out_size;
	pthread_mutex_lock(&mutex);
	rep = firstn->stable ? 0 : outpos;
	pthread_mutex_unlock(&mutex);
	int testnum = 0;
	ftotal = 0;
	skip_rep = 0;
	do {                                    //每次计算一个新的OSD，当前计算OSD失败或冲突时的循环,随机因子r改变
		retry_descent = 0;              //重置尝试次数
		in = firstn->bucket;              /* initial bucket */  //每次计算一个新的OSD时，设置当前开始查找的bucket为函数输入的bucket
		// if(in->id < 0)
		/* choose through intervening buckets */
		flocal = 0;
		do {                              //当前item type不是OSD时的循环，当前进行选择的bucket，即in改变。
			collide = 0;
			retry_bucket = 0;
			r = rep + parent_r;           //设置随机因子r
			/* r' = r + f_total */
			//cout<<pthread_self()<<" "<<r<<" "<<ftotal<<endl;
			r += ftotal;
			
			/* bucket choose */
			if (in->size == 0) {
				reject = 1;
				goto reject;
			}
			
			//在当前开始查找的in下选择一个item
			item = crush_bucket_choose(in, nullptr, firstn->x, r, (firstn->choose_args ? &firstn->choose_args[-1-in->id] : 0),firstn->outpos);
			//cout<<item<<endl;
			// cout<<item<<endl;
			if (item >= firstn->map->max_devices) {      //map->max_devices是最大的数
				skip_rep = 1;
				break;
			}

			/* desired type? */    //获取item类型
			if (item < 0)       //非osd的节点item都小于0
				itemtype = firstn->map->buckets[-1-item]->type;    //通过-1-item可以获取当前bucket的子bucket的类型
			else
				itemtype = 0;    //osd的id都大于等于0
			/* keep going? */   //判断计算得到的item是否输入的type
			if (itemtype != firstn->type) {
				if (item >= 0 ||(-1-item) >= firstn->map->max_buckets) 
				{
					skip_rep = 1;
					break;
				}
				in = firstn->map->buckets[-1-item];     //计算得到的item不是输入的type，获取当前bucket的item结构
				retry_bucket = 1;
				//此时的item必不是osd
				//printf("item = %d\n", item);
				continue;    //继续循环，找到osd为止
			}
			//printf("item = %d\n", item);
			//此时的item必是osd
			/* collision? */    //item是否与osd[]里面的osd相同？
			pthread_mutex_lock(&mutex);
			// cout<<pthread_self()<<" "<<pthread_outpos<<" "<<item<<endl;
			for (i = 0; i < outpos; i++) {
				//cout<<i<<" "<<firstn->out[i]<<endl;
				if (firstn->out[i] == item) {
					collide = 1;
					break;
				}
			}
			pthread_mutex_unlock(&mutex);

			reject = 0;
			if (!collide && firstn->recurse_to_leaf) {    //无冲突，进入判断是否在故障域内
				if (item < 0) {
					int sub_r;
					if (firstn->vary_r)
						sub_r = r >> (firstn->vary_r-1);
					else
						sub_r = 0;
					if (crush_choose_firstn(          //容灾域故障，重置期望类型为0，直至找到osd,递归调用crush_choose_firstn算法
							firstn->map,
							firstn->work,
							firstn->map->buckets[-1-item],
							weight, 
							weight_max,
							firstn->x, 
							firstn->stable ? 1 : firstn->outpos+1, 
							0,
							firstn->out2, 
							firstn->outpos, 
							count,
							firstn->recurse_tries, 
							0,
							firstn->local_retries,
							firstn->local_fallback_retries,
							0,
							firstn->vary_r,
							firstn->stable,
							NULL,
							sub_r,
							firstn->choose_args) <= firstn->outpos)
						/* didn't get leaf */
						reject = 1;
				} else {                          //无容灾域故障，将item放入输出out2数组中
				/* we already have a leaf! */
					pthread_mutex_lock(&mutex);
					firstn->out2[outpos] = item;
					pthread_mutex_unlock(&mutex);
				}
			}
			if (!reject && !collide) {    //无冲突，进入判断osd是否过载
				/* out? */
				if (itemtype == 0)   //type 0 osd
				{
					//cout<<firstn->map<<" "<<firstn->weight<<" "<<weight_max<<" "<<item<<" "<<firstn->x<<endl;
					reject = is_out(firstn->map, firstn->weight,weight_max,item, firstn->x);
				}
			}
reject:
			if (reject || collide) {
				ftotal++;
				flocal++;

				if (collide && flocal <= firstn->local_retries)
					/* retry locally a few times */
					retry_bucket = 1;
				else if (firstn->local_fallback_retries > 0 &&
						flocal <= in->size + firstn->local_fallback_retries)
					/* exhaustive bucket search */
					retry_bucket = 1;
				else if (ftotal < firstn->tries)
				{
					/* then retry descent */
					retry_descent = 1;
				}
				else
					/* else give up */
					skip_rep = 1;
			}
		} while (retry_bucket);
	} while (retry_descent);
	pthread_mutex_lock(&mutex);
	firstn->out[outpos] = item;
	firstn->outpos++;
	pthread_mutex_unlock(&mutex);
	//pthread_exit(NULL);
}

void* do_work3(void *arg)
{
	struct firstn_info* firstn = (struct firstn_info*)arg;
	//cout<<pthread_self()<<" "<<firstn->parent_r<<endl;
	int parent_r = 2;
	int outpos = 2;
	//cout<<pthread_self()<<" "<<firstn->x<<endl;
	int rep;    //计数器
	unsigned int ftotal, flocal;
	int retry_descent, retry_bucket, skip_rep;
	const struct crush_bucket *in = firstn->bucket;
	int r;
	int i;
	int item = 0;   //bucket的序号
	int itemtype;
	int collide, reject;
	int count = firstn->out_size;
	pthread_mutex_lock(&mutex);
	rep = firstn->stable ? 0 : outpos;
	pthread_mutex_unlock(&mutex);
	int testnum = 0;
	ftotal = 0;
	skip_rep = 0;
	do {                                    //每次计算一个新的OSD，当前计算OSD失败或冲突时的循环,随机因子r改变
		retry_descent = 0;              //重置尝试次数
		in = firstn->bucket;              /* initial bucket */  //每次计算一个新的OSD时，设置当前开始查找的bucket为函数输入的bucket
		// if(in->id < 0)
		/* choose through intervening buckets */
		flocal = 0;
		do {                              //当前item type不是OSD时的循环，当前进行选择的bucket，即in改变。
			collide = 0;
			retry_bucket = 0;
			r = rep + parent_r;           //设置随机因子r
			/* r' = r + f_total */
			//cout<<pthread_self()<<" "<<r<<" "<<ftotal<<endl;
			r += ftotal;
			
			/* bucket choose */
			if (in->size == 0) {
				reject = 1;
				goto reject;
			}
			
			//在当前开始查找的in下选择一个item
			item = crush_bucket_choose(in, nullptr, firstn->x, r, (firstn->choose_args ? &firstn->choose_args[-1-in->id] : 0),firstn->outpos);
			//cout<<item<<endl;
			// cout<<item<<endl;
			if (item >= firstn->map->max_devices) {      //map->max_devices是最大的数
				skip_rep = 1;
				break;
			}

			/* desired type? */    //获取item类型
			if (item < 0)       //非osd的节点item都小于0
				itemtype = firstn->map->buckets[-1-item]->type;    //通过-1-item可以获取当前bucket的子bucket的类型
			else
				itemtype = 0;    //osd的id都大于等于0
			/* keep going? */   //判断计算得到的item是否输入的type
			if (itemtype != firstn->type) {
				if (item >= 0 ||(-1-item) >= firstn->map->max_buckets) 
				{
					skip_rep = 1;
					break;
				}
				in = firstn->map->buckets[-1-item];     //计算得到的item不是输入的type，获取当前bucket的item结构
				retry_bucket = 1;
				//此时的item必不是osd
				//printf("item = %d\n", item);
				continue;    //继续循环，找到osd为止
			}
			//printf("item = %d\n", item);
			//此时的item必是osd
			/* collision? */    //item是否与osd[]里面的osd相同？
			pthread_mutex_lock(&mutex);
			// cout<<pthread_self()<<" "<<pthread_outpos<<" "<<item<<endl;
			for (i = 0; i < outpos; i++) {
				//cout<<i<<" "<<firstn->out[i]<<endl;
				if (firstn->out[i] == item) {
					collide = 1;
					break;
				}
			}
			pthread_mutex_unlock(&mutex);

			reject = 0;
			if (!collide && firstn->recurse_to_leaf) {    //无冲突，进入判断是否在故障域内
				if (item < 0) {
					int sub_r;
					if (firstn->vary_r)
						sub_r = r >> (firstn->vary_r-1);
					else
						sub_r = 0;
					if (crush_choose_firstn(          //容灾域故障，重置期望类型为0，直至找到osd,递归调用crush_choose_firstn算法
							firstn->map,
							firstn->work,
							firstn->map->buckets[-1-item],
							weight, 
							weight_max,
							firstn->x, 
							firstn->stable ? 1 : firstn->outpos+1, 
							0,
							firstn->out2, 
							firstn->outpos, 
							count,
							firstn->recurse_tries, 
							0,
							firstn->local_retries,
							firstn->local_fallback_retries,
							0,
							firstn->vary_r,
							firstn->stable,
							NULL,
							sub_r,
							firstn->choose_args) <= firstn->outpos)
						/* didn't get leaf */
						reject = 1;
				} else {                          //无容灾域故障，将item放入输出out2数组中
				/* we already have a leaf! */
					pthread_mutex_lock(&mutex);
					firstn->out2[outpos] = item;
					pthread_mutex_unlock(&mutex);
				}
			}
			if (!reject && !collide) {    //无冲突，进入判断osd是否过载
				/* out? */
				if (itemtype == 0)   //type 0 osd
				{
					//cout<<firstn->map<<" "<<firstn->weight<<" "<<weight_max<<" "<<item<<" "<<firstn->x<<endl;
					reject = is_out(firstn->map, firstn->weight,weight_max,item, firstn->x);
				}
			}
reject:
			if (reject || collide) {
				ftotal++;
				flocal++;

				if (collide && flocal <= firstn->local_retries)
					/* retry locally a few times */
					retry_bucket = 1;
				else if (firstn->local_fallback_retries > 0 &&
						flocal <= in->size + firstn->local_fallback_retries)
					/* exhaustive bucket search */
					retry_bucket = 1;
				else if (ftotal < firstn->tries)
				{
					/* then retry descent */
					retry_descent = 1;
				}
				else
					/* else give up */
					skip_rep = 1;
			}
		} while (retry_bucket);
	} while (retry_descent);
	pthread_mutex_lock(&mutex);
	firstn->out[outpos] = item;
	firstn->outpos++;
	pthread_mutex_unlock(&mutex);
	//pthread_exit(NULL);
}
struct firstn_info2
{
	const struct crush_map *map;
	const struct crush_bucket *bucket;
	int x;
};

void* do_work4(void* arg)
{
	struct firstn_info2* firstn = (struct firstn_info2*)arg;
	struct crush_work *work = nullptr;
	int x = firstn->x;
	int numrep = 3;
	int *out = new int[numrep];
	int recurse_tries = 1;
	int j = 0;
	int choose_tries = firstn->map->choose_total_tries + 1;
	struct crush_choose_arg *choose_args = nullptr;
	crush_choose_firstn(firstn->map,
						work,firstn->map->buckets[0],
						reweight,
						weight_max,
						x,
						numrep,
						j,
						out,
						0,
						3,
						choose_tries,
						recurse_tries,
						firstn->map->choose_local_tries,
						firstn->map->choose_local_fallback_tries,
						1,
						firstn->map->chooseleaf_vary_r,
						firstn->map->chooseleaf_stable,
						out,
						0,
						choose_args);
	//计算三个osd，三个正整数，存放系out[]里面
	// cout<<"osd: ";
	// for(int i = 0;i < 3;++i)
	// {
	// 	cout<<out[i]<<" ";
	// }
	// cout<<endl;
	delete[] out;
	pthread_exit(NULL);
}

//多线程的crush算法
static int pthread_crush_choose_firstn(struct firstn_info& firstn)
{
    
	pthread_t tid;
	int *out = new int[firstn.numrep];
	firstn.out = out;
	firstn.out2 = out;
	firstn.outpos = 0;
	pthread_create(&tid,NULL,do_work1,(void *)&firstn);
	//pthread_join(tid,NULL);
    pthread_detach(tid);

	pthread_create(&tid,NULL,do_work2,(void *)&firstn);
	//pthread_join(tid,NULL);
    pthread_detach(tid);

	pthread_create(&tid,NULL,do_work3,(void *)&firstn);
	//pthread_join(tid,NULL);
	pthread_detach(tid);
	while(firstn.outpos != 3){}
	// cout<<"osd: ";
	// for(int i = 0;i < 3;++i)
	// {
	// 	cout<<out[i]<<" ";
	// }
	// cout<<endl;
	delete[] out;

	return firstn.outpos;
}

void test(const struct crush_map *map,struct crush_bucket_straw2* *buckets)
{
	struct crush_work *work = nullptr;
	auto start = chrono::steady_clock::now();
	for(int i = 0; i < object_num; ++i)   //1G大小的文件可以分成256个object
	{
		int x = 222+i;
		int numrep = 3;
		int *out = new int[numrep];
		int recurse_tries = 1;
		int j = 0;
		int choose_tries = map->choose_total_tries + 1;
		struct crush_choose_arg *choose_args = nullptr;
		crush_choose_firstn(map,
							work,map->buckets[0],
							reweight,
							weight_max,
							x,
							numrep,
							j,
							out,
							0,
							3,
							choose_tries,
							recurse_tries,
							map->choose_local_tries,
							map->choose_local_fallback_tries,
							1,
							map->chooseleaf_vary_r,
							map->chooseleaf_stable,
							out,
							0,
							choose_args);
		//计算三个osd，三个正整数，存放系out[]里面
		// cout<<"osd: ";
		// for(int i = 0;i < 3;++i)
		// {
		// 	cout<<out[i]<<" ";
		// }
		// cout<<endl;
		delete[] out;
	}
	auto end = chrono::steady_clock::now();
    auto diff = end - start;
    cout<<__func__<<" CRUSH RunTime:\t\t" << chrono::duration <double, milli> (diff).count()<<"ms"<<endl;
}


void test2(const struct crush_map *map,struct crush_bucket_straw2* *buckets)
{
	auto start = chrono::steady_clock::now();
	for(int i = 0; i < object_num; ++i)   //1G大小的文件可以分成256个object
	{
		pthread_t tid;
		struct firstn_info2 firstn;
		firstn.map = map;
		firstn.bucket = map->buckets[0];
		firstn.x = 222+i;
		pthread_create(&tid,NULL,do_work4,(void *)&firstn);
		pthread_join(tid,NULL);
		//pthread_detach(tid);
	}
	auto end = chrono::steady_clock::now();
    auto diff = end - start;
    cout<<__func__<<" CRUSH RunTime:\t\t" << chrono::duration <double, milli> (diff).count()<<"ms"<<endl;
}


void test_pthread(const struct crush_map *map,struct crush_bucket_straw2* *buckets)
{
	struct crush_work *work = nullptr;
	int numrep = 3;
	int recurse_tries = 1;
	int j = 0;
	int choose_tries = map->choose_total_tries + 1;
	struct crush_choose_arg *choose_args = nullptr;
	struct firstn_info firstn;
	
	firstn.map = map;
	firstn.work = work;
	firstn.bucket = map->buckets[0];
	firstn.weight = reweight;
	firstn.weight_max = weight_max;
	firstn.numrep = numrep;
	firstn.type = 0;

	firstn.outpos = 0;
	firstn.out_size = 3;
	firstn.tries = choose_tries;
	firstn.recurse_tries = recurse_tries;
	firstn.local_retries = map->choose_local_tries;
	firstn.local_fallback_retries = map->choose_local_fallback_tries;
	firstn.recurse_to_leaf = 1;

	firstn.vary_r = map->chooseleaf_vary_r;
	firstn.stable = map->chooseleaf_stable;
	firstn.parent_r = 0;
	firstn.choose_args = choose_args;
	auto start = chrono::steady_clock::now();
	for(int i = 0; i < object_num; ++i)   //1G大小的文件可以分成256个object
	{
		firstn.x = 222+i;
		pthread_crush_choose_firstn(firstn);
	}
	auto end = chrono::steady_clock::now();
    auto diff = end - start;
    cout<<__func__<<" CRUSH RunTime:\t" << chrono::duration <double, milli> (diff).count()<<"ms"<<endl;
}


static int pthreadPool_crush_choose_firstn(struct firstn_info* firstn,ThreadPool* threadpool)
{
	//传入传出参数
    int *out = new int[firstn->numrep]();
	firstn->out = out;
	firstn->out2 = out;
	pthread_t tid;
	firstn->outpos = 0;
	threadPoolAdd(threadpool,do_work1,(void *)firstn);
	threadPoolAdd(threadpool,do_work2,(void *)firstn);
	threadPoolAdd(threadpool,do_work3,(void *)firstn);
	//cout<<pthread_outpos<<endl;
	while(firstn->outpos != 3){}
	// cout<<"osd: ";
	// for(int i = 0;i < 3;++i)
	// {
	// 	cout<<firstn->out[i]<<" ";
	// }
	// cout<<endl;
	delete []firstn->out;
	return firstn->outpos;
}


void test_pthreadPool(const struct crush_map *map,struct crush_bucket_straw2* *buckets)
{
	struct crush_work *work = nullptr;
	ThreadPool* threadpool = threadPoolCreate(3,10,10);
	
	int numrep = 3;
	int recurse_tries = 1;
	int j = 0;
	int choose_tries = map->choose_total_tries + 1;
	struct crush_choose_arg *choose_args = nullptr;
	struct firstn_info* firstn = new firstn_info();
	//传入参数
	firstn->map = map;
	firstn->work = work;
	firstn->bucket = map->buckets[0];
	firstn->weight = reweight;
	firstn->weight_max = weight_max;
	firstn->numrep = numrep;
	firstn->type = 0;

	firstn->outpos = 0;
	firstn->out_size = 3;
	firstn->tries = choose_tries;
	firstn->recurse_tries = recurse_tries;
	firstn->local_retries = map->choose_local_tries;
	firstn->local_fallback_retries = map->choose_local_fallback_tries;
	firstn->recurse_to_leaf = 1;

	firstn->vary_r = map->chooseleaf_vary_r;
	firstn->stable = map->chooseleaf_stable;
	firstn->parent_r = 0;
	firstn->choose_args = choose_args;
	auto start = chrono::steady_clock::now();
	for(int i = 0; i < object_num; ++i)   //1G大小的文件可以分成256个object
	{
		firstn->x = 222+i;
		pthreadPool_crush_choose_firstn(firstn, threadpool);
	}
	auto end = chrono::steady_clock::now();
    auto diff = end - start;
    cout<<__func__<<" CRUSH RunTime:\t" << chrono::duration <double, milli> (diff).count()<<"ms"<<endl;
	threadPoolDestroy(threadpool);
}


int main()
{
	pthread_mutex_init(&mutex,NULL);
    struct crush_map* map = new crush_map();
	struct crush_bucket_straw2* *buckets = new crush_bucket_straw2*[1000];
	object_num = 256;
    init2(map,buckets);//环境
	cout<<"crush_map initialization is complete ......"<<endl;
	for(int i = 0; i < map->max_buckets;++i)
	{
		cout<<"i = "<< i <<";\t";
		cout<<"id = "<< buckets[i]->h.id<<";\t";
		cout<<"type = "<<  buckets[i]->h.type<<";\t";
		cout<<"weight = "<<  buckets[i]->h.weight<<";\t";
		cout<<"size = "<<  buckets[i]->h.size<<";\titem: ";
		for(int j = 0;j < buckets[i]->h.size;++j)
		{
			cout<<buckets[i]->h.items[j]<<" ";
		}
		// cout<<"\tweight: ";
		// for(int j = 0;j < buckets[i]->h.size;++j)
		// {
		// 	cout<<buckets[i]->item_weights[j]<<" ";
		// }
		cout<<endl;
	}

	test(map,buckets);
	test2(map,buckets);
	test_pthread(map,buckets);
	test_pthreadPool(map,buckets);
	
    return 0;
}

