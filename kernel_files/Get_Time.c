#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/ktime.h>

asmlinkage int Get_Time(long long int *a,long long int *b)
{
	struct timespec t;
	getnstimeofday(&t);
	*a = t.tv_sec;
	*b = t.tv_nsec; 
    return 0;
}
