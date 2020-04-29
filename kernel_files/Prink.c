#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage int Prink(char *MSG)
{
	printk(MSG);
    return 0;
}
