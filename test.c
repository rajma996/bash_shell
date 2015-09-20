#include<unistd.h>
#include<stdio.h>
#include<signal.h>
int main()
{
	int pid = fork();
	if(pid==0)
	{
		printf("Child process  id %d\n",getpid());
		printf("Child process group  id %d\n",getpgid(getpid()));
		printf("Child parnet  id %d\n",getppid());
	}
	else
	{
		printf("parent process  id %d\n",getpid());
		printf(" parentprocess group  id %d\n",getpgid(getpid()));
		printf(" parentparnet  id %d\n",getppid());

	}
}
