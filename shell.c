#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

char* home;
char *input;
pid_t shell_id;
pid_t shell_pgid;
int background;
int fd_shell;

typedef struct command
{
	int argc;
	char *argv[100];
	struct command* next;
	char* infile; char* outfile;
}command;

typedef struct process
{
	char *name;
	int pid;
	struct process* next;
}process;	

command* command_head=NULL;
process* process_head=NULL;

void getprompt()
{
//	printf("prompt called\n");
	char username[32];
	getlogin_r(username,32);
	char host[100];
	gethostname(host,100);
	char dir[1000];
	getcwd(dir,1000);
	char display_dir[1000];
	char* temp=strstr(dir,home);
	if (temp)
	{
		printf("%s\n",temp);
	strcpy(display_dir,dir+strlen(home));
		printf("%s@%s:~%s",username,host,display_dir);
	}
	else
	{
		strcpy(display_dir,dir);
		printf("%s@%s:%s ",username,host,display_dir);
	}
}

void insert_command(int argc,char *argv[100],char* input,char* output)
{
	command* temp=malloc(sizeof(command));
	int i;
	for(i=0;i<argc;i++)
	{
		temp->argv[i]=malloc(sizeof(char)*100);
		strcpy(temp->argv[i],argv[i]);
	}
	temp->argc=argc;
	if(input!=NULL)
	{
		temp->infile=malloc(100);
		strcpy(temp->infile,input);
	}
	else
		temp->infile=NULL;
	if(output!=NULL)
	{
		temp->outfile=malloc(100);
		strcpy(temp->outfile,output);
	}
	else
		temp->outfile=NULL;
	/*printf("%d\n",temp->argc);
	  for(i=0;i<temp->argc;i++) printf("%s\n",temp->argv[i]);
	  i	printf("\n");*/
	if (command_head==NULL)
	{
		command_head=temp;
		return;
	}
	else 
	{
		command* temp1=command_head;
		while(temp1->next!=NULL) temp1=temp1->next;
		temp1->next=temp;
		return;
	}
}
void insert_process(char *name,int pid)
{
	process* temp=malloc(sizeof(process));
	temp->name=malloc(100);
	strcpy(temp->name,name);
	temp->pid=pid;
	temp->next=NULL;
	if(process_head==NULL) process_head=temp;
	else
	{
		process* t=process_head;
		while(t->next!=NULL) t=t->next;
		t->next=temp;
		return;
	}
}
process* get_process_pid(int pid)
{
	process* temp=process_head;
	while(temp!=NULL)
	{
		if(temp->pid==pid) return temp;
		temp=temp->next;
	}
	return  NULL;
}
process* get_process_job(int n)
{
	process* temp=process_head;
	while(1) 
	{
		if(n==1) return temp;
		if(temp==NULL) return NULL;
		n--;
		temp=temp->next;
	}
	return temp;
}
void sighandler(int sig)
{
	// if(sig==SIGINT)
	// {
	// 	printf("sdfkj\n");
	// 	getprompt();
	// }
	if(sig==SIGCHLD)
	{
		printf("sigchild called\n");
		int pid,status;
		while(pid=waitpid(-1,&status,WNOHANG)>0)
		{
			printf("process with pid %d exited \n",pid);
			if(pid!=-1 && pid!=0)
			{
				if(WIFEXITED(status))				
				{
					process* temp=get_process_pid(pid);
					if(temp!=NULL)
					{
						printf("%s with pid %d exited normally\n",temp->name,temp->pid);
						//remove_process(pid);
					}
				}
				else if(WIFSIGNALED(status))
				{
					process* temp=get_process_pid(pid);
					if(temp!=NULL)
					{
						printf("%s with pid %d signalled to exit\n",temp->name,temp->pid);
						//remove_process(pid);
					}
				}
			}
		 }		
	}
}

void init()
{
	home=malloc(100);
	home=getcwd(home,1000);
	fd_shell=STDERR_FILENO;background=0;				
	signal (SIGINT, SIG_IGN);
	signal (SIGTSTP, SIG_IGN);
	signal (SIGQUIT, SIG_IGN);
	signal (SIGTTIN, SIG_IGN);
	signal (SIGTTOU, SIG_IGN);
	shell_pgid=getpid();
	if(setpgid(shell_pgid,shell_pgid)<0)
	{
		perror("Can't make shell a member of it's own process group");
		_exit(1);
	}
	tcsetpgrp(fd_shell,shell_pgid);	
}
void print_process()
{
	int i=0,j;
	process* temp=process_head;
	while(temp!=NULL)
	{
		printf("%d\n",i);	
		printf("pid %d\n",temp->pid);
		printf("pgid %d\n",getpgid(temp->pid));		
		printf("%s\n",temp->name);
		temp=temp->next;
	}
}

void print_command()
{
	int i=0,j;
	command* temp=command_head;
	while(temp!=NULL)
	{
		printf("%d\n",i);	
		printf("argc %d",temp->argc);
		for(j=0;j<temp->argc;j++) printf("%d %s \n",j,temp->argv[j]);
		if(temp->infile) printf(" infile %s\n",temp->infile);
		if(temp->outfile) printf(" outfile %s\n",temp->outfile);
		temp=temp->next; i++;
	}
}


int parse(char* input)
{
	int count=0;
	char* savepointer1;
	char* savepointer2;
	char* token=strtok_r(input,"|",&savepointer1);
	while(token!=NULL)
	{
		count++;
		int argc=0;
		char* input=NULL;
		char* output=NULL;
		char *argv[100];
		int i; for(i=0;i<100;i++) argv[i]=NULL;
		char* temp_token=strtok_r(token," ",&savepointer2);
		while(temp_token!=NULL)
		{	
			if(strcmp(temp_token,"<")==0)
			{
				temp_token=strtok_r(NULL," ",&savepointer2);
				if(temp_token==NULL) 
				{
					perror("input file not given");
				}
				else 
				{
					input=malloc(100);
					strcpy(input,temp_token);
					temp_token=strtok_r(NULL," ",&savepointer2);
					continue;
				}
			}
			else if(strcmp(temp_token,">")==0)
			{
				temp_token=strtok_r(NULL," ",&savepointer2);
				if(temp_token==NULL) 
				{
					perror("output file not given");
				}
				else 
				{
					output=malloc(100);
					strcpy(output,temp_token);
					temp_token=strtok_r(NULL," ",&savepointer2);
					continue;
				}

			}
			argv[argc]=malloc(sizeof(char)*100);
			strcpy(argv[argc++],temp_token);
			temp_token=strtok_r(NULL," ",&savepointer2);
		}
		insert_command(argc,argv,input,output);
		token=strtok_r(NULL,"|",&savepointer1);
	}
	return count;
}

void run(command* temp)
{
		if(temp->infile!=NULL) 
		{
			int in = open(temp->infile,O_RDONLY);
			dup2(in,0);
			close(in);
		}
		else if (temp->outfile!=NULL)
		{
			int out = open(temp->outfile,O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
			dup2(out,1);
			close(out);
		}
		execvp(temp->argv[0],temp->argv);
		
}

void execute()
{
	
	command* temp=command_head;
	int ret_value;
	if(strcmp(temp->argv[0],"jobs")==0)
	{
		int i=0;
		process* temp=process_head;
		while(temp!=NULL)
		{
			printf("[%d] %s [%d]\n",i+1,temp->name,temp->pid);
			temp=temp->next;
		}
	}
	else if(strcmp(temp->argv[0],"kjob")==0)
	{
		if (temp->argc!=3) 
		{
			printf("kjob takes 2 arguments. %d given",temp->argc-1);
		}
		else
		{
			process* temp1=get_process_job(atoi(temp->argv[1]));
			if(temp1!=NULL)	kill(temp1->pid,atoi(temp->argv[2]));
			else printf("job id not valid\n");
		}
	}
	else if(strcmp(temp->argv[0],"fg")==0)
	{
		if(temp->argc!=2)
		{
			printf("fg takes 1 argument %d given",temp->argc-1);
		}
		else
		{
			process* temp1=get_process_job(atoi(temp->argv[1]));
			if(temp==NULL)
			{
				printf("NO such process in background");
			}
			else
			{
				tcsetpgrp(fd_shell,getpgid(temp1->pid));
				waitpid(temp1->pid,NULL,WUNTRACED);
			}
		}
	}
	else if(strcmp(temp->argv[0],"overkill")==0)
	{
		while(process_head!=NULL)
		{
			kill(process_head->pid,SIGKILL);
			process_head=process_head->next;
		}
	}
	else if(strcmp(temp->argv[0],"pwd")==0 )
	{
		printf("%s\n",getcwd(NULL,0));
	}
	else if (strcmp(temp->argv[0],"echo")==0)
	{
		int j;
		for(j=1;j<temp->argc;j++)
		{
			printf("%s ",temp->argv[j]);
		}
		printf("\n");
	}
	else if (strcmp(temp->argv[0],"cd")==0)
	{
		if(temp->argv[1]==NULL) ret_value=chdir(home);
		else if(temp->argv[1][0]=='~') 
		{
			char ch_dir[1000]; 
			int j,l1=strlen(temp->argv[1]),l2=strlen(home);
			strcpy(ch_dir,home);
			for(j=1;j<l1;j++) ch_dir[j+l2-1]=temp->argv[1][j];
			ch_dir[l2+j-1]='\0';
			chdir(ch_dir);
		}
		else ret_value=chdir(temp->argv[1]);
		if(ret_value==-1)
		{
			printf("No such directory exists\n");
		}
	}
	else if(strcmp(temp->argv[0],"exit")==0)
	{
		_exit(0);
	}
	else
	{
		int pid=fork();
		if(pid==0)
		{
			setpgid(getpid(),getpid());
			if(background==0) 
				tcsetpgrp(fd_shell,getpid());
			signal (SIGINT, SIG_DFL);
			signal (SIGQUIT, SIG_DFL);
			signal (SIGTSTP, SIG_DFL);
			signal (SIGTTIN, SIG_DFL);
			signal (SIGTTOU, SIG_DFL);
			signal (SIGCHLD, SIG_DFL);
			run(temp);
			exit(0);
		}
		if (background==0)
		{
			int status;
			tcsetpgrp(fd_shell,pid);
			waitpid(pid,&status,WUNTRACED);
			// if(!WIFSTOPPED(status))
			// {
			// 	//remove_process(pid);
			// }
			// else
			// 	printf("\n[%d]+ stopped %s\n",pid,temp->argv[0]);
			tcsetpgrp(fd_shell,shell_pgid);
		}
		else if(background==1)
		{
			printf("Background process %s wtih id %d created\n",temp->argv[0],pid);
			insert_process(temp->argv[0],pid);
		}
	}

}
void piped_execute(int n)
{
//	printf("n:%d\n",n);
	int pid[n-1];
	int i=0,j=0;
	int pipes[n][2];
	for(i=0;i<n;i++) if(pipe(pipes[i])<0)perror("pipe error\n");
	command* temp = command_head;
	i=0;
	//for(j=0;j<2*(n-1);j++) close(pipes[j]);
	while(temp!=NULL)
	{
		int k;
//		for(k=0;k<temp->argc;k++) printf("%s\n",temp->argv[k]);
		pid[i]=fork();
		if(pid[i]<0)printf("child error\n");
		if(pid[i]==0)
		{
			if(i!=0)
			{
				dup2(pipes[i-1][0],0);
				if(i!=n-1)dup2(pipes[i][1],1);
			}
			else 
			{
				dup2(pipes[0][1],1);
			}
			for(j=0;j<n;j++){close(pipes[j][0]);close(pipes[j][1]);}
			run(temp);
			//i++;
			//temp=temp->next;
			//for(j=0;j<2*(n-1);j++) close(pipes[j]);
		}
		i++;
		temp=temp->next;
	}
	for(j=0;j<n;j++){close(pipes[j][0]);close(pipes[j][1]);}
	//for(j=0;j<i;j++) waitpid(pid[j],NULL,0);
}
void delete_command(command* head)
{
	if(head==NULL) return;
	if(head->next==NULL) { free(head); return ; }
	delete_command(head->next);
	free(head);
}
void delete_process(process* head)
{
	if(head==NULL) return;
	if(head->next==NULL) { free(head); return ; }
	delete_process(head->next);
	free(head);
}
int main()
{
	init();
//	signal(SIGINT,sighandler);
	input=malloc(sizeof(char)*1000)	;
//	printf("%d\n",getpid());
	while(1)
	{
		signal(SIGCHLD,sighandler);
		background=0;
		getprompt();
		scanf(" %[^\n]",input);
		printf(" input  %s\n",input);
		int i;
		for(i=strlen(input);i>=0;i--)
		{
			if(input[i]==' ') continue;
			else if(input[i]=='&') { input[i]=' '; background=1; break; }
		}
		int num=parse(input);
		printf("num %d\n",num);
		print_command();		print_process();

		if (num==1) execute();
		else piped_execute(num);
		fflush(stdout);
		command_head=NULL;
		process_head=NULL;
//		sleep(2);
		//delete_command(command_head);
		//delete_process(process_head);
	}
}
 