#include <dirent.h>
#include "utils.h"
#include <unistd.h>
//перед добавлением зависимых служб прочесать availdir и искать в этом списке
#define START 1<<0
#define STOP 1<<1
#define FULL 1<<2

typedef struct
{
	char *name;
	char **deps;
	int numdeps;
} service;

static void checkfull();
static int checksorted(service *list, int n);
static service init(char *name);
static void sort(service *list, int n, int begin);
static void usage(char *name);

static int mode=0;
static char *rundir="/etc/sidal/run", *availdir="/etc/sidal/avail";
static service *list;
static int nsvcs=0;

int
main(int argc, char *argv[])
{
	char **svcs=(char**)malloc(0);
	int i, j, k;
	for(i=1;i<argc;i+=1) {
		if (argv[i][0]=='-') {
			j=0;
			if (!strcmp(argv[i],"--help") || strchr(argv[i],'h')) {
				usage(argv[0]);
			}
			if (strchr(argv[i],'s') || !strcmp(argv[i],"--start")) {
				mode=(mode&!STOP)|START;
				j=1;
			}
			if (strchr(argv[i],'k') || !strcmp(argv[i],"--stop")) {
				mode=(mode&!START)|STOP;
				j=1;
			}
			if (strchr(argv[i],'a')) {
				availdir=argv[++i];
				j=1;
			} else if (strchr(argv[i],'d')) {
				rundir=argv[++i];
				j=1;
			}
			if (strchr(argv[i],'f')) {
				mode=mode|FULL;
				j=1;
			} 
			if (!j) {
				usage(argv[0]);
			}
		} else {
			svcs=(char**)realloc(svcs,(nsvcs+1)*sizeof(char*));
			svcs[nsvcs]=argv[i];
			nsvcs+=1;
		}
	}
	if (!nsvcs) {
		die("no services given\n");
	}
	list=(service*)malloc(nsvcs*sizeof(service));
	
	for(i=0;i<nsvcs;i+=1) {
		list[i]=init(svcs[i]);
	}
	if (mode&FULL) {
		checkfull();
	}
	j=-1;
	k=0;
	while((i=checksorted(list,nsvcs))<nsvcs) {
		if (i==j) {
			k+=1;
			if (k==nsvcs) {
				perror("cycling dependencies, no further progress may be done!\n");
				break;
			}
		}
		else k=0;
		j=i;
		
		sort(list, nsvcs, i);
	}
	if (mode&START) for(i=0;i<nsvcs;i+=1) printf("%s\n",list[i].name);
	else for(i=nsvcs-1;i>=0;i-=1) printf("%s\n",list[i].name);
	for(i=0;i<nsvcs;i+=1) {
		for(j=0;j<list[i].numdeps;j+=1)
			free(list[i].deps[j]);
		free(list[i].deps);
	}
	free(list);
	return 0;
}

void
checkfull()
{
	int i, j, k, nrun, navail;
	int provided;
	service *run, *avail;
	DIR *tolist;
	struct dirent *srv;
	
	nrun=0;
	navail=0;
	run=(service*)malloc(0);
	avail=(service*)malloc(0);
	tolist=opendir(rundir);
	readdir(tolist);/* skip . and .. */
	readdir(tolist);
	while((srv=readdir(tolist))) {
		nrun+=1;
		run=(service*)realloc(run,sizeof(service)*nrun);
		run[nrun-1]=init(srv->d_name);
	}
	closedir(tolist);
	tolist=opendir(availdir);
	readdir(tolist);/* skip . and .. */
	readdir(tolist);
	while((srv=readdir(tolist))) {
		navail+=1;
		avail=(service*)realloc(avail,sizeof(service)*navail);
		avail[navail-1]=init(srv->d_name);
	}
	closedir(tolist);
	for (i=0;i<nsvcs;i+=1) {
		for (j=0;j<list[i].numdeps;j+=1) {
			provided=0;
			for (k=0;k<nsvcs;k+=1) {
				if (!strcmp(list[k].name,list[i].deps[j])) {
					provided=1;
					break;
				}
			}
			if (provided) continue;
			if (!provided) {
				for (k=0;k<nrun;k+=1) {
					if (!strcmp(list[i].deps[j],run[k].name)) {
						provided=1;
					}
					if (provided) {
						nsvcs+=1;
						list=(service*)realloc(list,nsvcs*sizeof(service));
						list[nsvcs-1]=init(run[k].name);
						break;
					}
				}
			}
			if (!provided) {
				for (k=0;k<navail;k+=1) {
					if (!strcmp(list[i].deps[j],avail[k].name)) {
						nsvcs+=1;
						list=(service*)realloc(list,nsvcs*sizeof(service));
						list[nsvcs-1]=init(avail[k].name);
					}
				}
			}
		}
	}
	free(run);
	free(avail);
}

int
checksorted(service *list, int n)
{
	int i, j, k;
	for (i=0;i<n-1;i+=1) {
		if (list[i].numdeps<0) {
			for (j=i+1;j<n;j+=1) {
				if (list[j].numdeps>=0) {
					return i;
				}
			}
		} else for (j=i+1;j<n;j+=1) {
			for (k=0;k<list[i].numdeps;k+=1) {
				if (!strcmp(list[j].name,list[i].deps[k])) {
					return i;
				}
			}
		}
	}
	return n;
}

service
init(char *name)
{
	char buf[256];
	FILE *pipe;
	service ret;
	char *dir=NULL;
	
	ret.name=smprintf("%s",name);
	/* get info about dependencies */
	if ((pipe=fopen(smprintf("%s/%s",rundir,name),"r"))) {
		dir=rundir;
	} else if ((pipe=fopen(smprintf("%s/%s",availdir,name),"r"))) {
		dir=availdir;
	} else {
	/* if these services are not in /etc/sidal/run, mark them to run in the end */
		ret.deps=0;
		ret.numdeps=-1;
	}
	if (pipe) {
		fclose(pipe);
		pipe=popen(smprintf("%s/%s depend",dir,name),"r");
		ret.deps=(char**)malloc(0);
		ret.numdeps=0;
		while(fscanf(pipe,"%s",buf)!=EOF) {
			ret.deps=(char**)realloc(ret.deps,(ret.numdeps+1)*sizeof(char*));
			ret.deps[ret.numdeps]=smprintf("%s",buf);
			ret.numdeps+=1;
		}
		pclose(pipe);
	}
	
	return ret;
}

void
sort(service *list, int n, int begin)
{
	int i, j, k;
	service sw;
	for (i=begin;i<n-1;i+=1) {
		if (list[i].numdeps<0) {
			for (j=n-1;j>=i;j-=1)
				if (list[j].numdeps>=0)
					break;
			sw=list[i];
			list[i]=list[j];
			list[j]=sw;
		}
		for (j=i+1;j<n;j+=1) {
			for (k=0;k<list[i].numdeps;k+=1) {
				if (!strcmp(list[j].name,list[i].deps[k])) {
					sw=list[i];
					list[i]=list[j];
					list[j]=sw;
				}
			}
		}
	}
}

void
usage(char *name)
{
	die("usage: %s [-skf] [-ad directory] ...\n",name);
}
