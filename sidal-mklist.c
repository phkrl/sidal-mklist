#include <dirent.h>
#include "utils.h"
#include <unistd.h>
#define START 1<<0
#define STOP 1<<1
#define FULL 1<<2
#define LEVELLED 1<<3

typedef struct
{
	char *name;
	char **deps;
	int numdeps;
} service;

static void checkfull();
static int checksorted();
static service init(char *name);
static void mklevels();
static void sort(int begin);
static void usage(char *name);

static int mode=0;
static char *rundir="/etc/sidal/run", *availdir="/etc/sidal/avail", *servicedir="/run/sidal";
static service *list;
static int nsvcs=0;
static int nlevels=0, *levels=NULL;

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
			if (strchr(argv[i],'a') || !strcmp(argv[i],"--availdir")) {
				availdir=argv[++i];
				j=1;
			} else if (strchr(argv[i],'d') || !strcmp(argv[i],"--rundir")) {
				rundir=argv[++i];
				j=1;
			} else if (strchr(argv[i],'r') || !strcmp(argv[i],"--activedir")) {
				servicedir=argv[++i];
				j=1;
			}
			if (strchr(argv[i],'f') || !strcmp(argv[i],"--full")) {
				mode=mode|FULL;
				j=1;
			}
			if (strchr(argv[i],'l') || !strcmp(argv[i],"--levelled")) {
				mode=mode|LEVELLED;
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
	while((i=checksorted())<nsvcs) {
		if (i==j) {
			k+=1;
			if (k==nsvcs) {
				perror("cycling dependencies, no further progress may be done!\n");
				break;
			}
		}
		else k=0;
		j=i;
		
		sort(i);
	}
	
	if (mode&LEVELLED) mklevels();
	if (mode&START) {
		j=0;
		for(i=0;i<nsvcs;i+=1) {
			printf("%s ",list[i].name);
			if (mode&LEVELLED) {
				if (i==levels[j]) {
					printf("\n");
					j+=1;
				}
			}
		}
	} else {
		j=nlevels-1;
		for(i=nsvcs-1;i>=0;i-=1) {
			printf("%s ",list[i].name);
			if (mode&LEVELLED) {
				if (i==levels[j]) {
					printf("\n");
					j-=1;
				}
			}
		}
	}
	printf("\n");
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
	int i, j, k, nrun, navail, nstarted;
	int provided;
	service *run, *avail, *started;
	DIR *tolist;
	struct dirent *srv;
	
	nrun=0;
	navail=0;
	nstarted=0;
	run=(service*)malloc(0);
	avail=(service*)malloc(0);
	started=(service*)malloc(0);
	/* create list of started services */
	tolist=opendir(servicedir);
	/* skip . and .. */
	readdir(tolist);
	readdir(tolist);
	while((srv=readdir(tolist))) {
		nstarted+=1;
		started=(service*)realloc(started,sizeof(service)*nstarted);
		started[nstarted-1]=init(srv->d_name);
	}
	closedir(tolist);
	/* create list of services expected to start */
	tolist=opendir(rundir);
	/* skip . and .. */
	readdir(tolist);
	readdir(tolist);
	while((srv=readdir(tolist))) {
		nrun+=1;
		run=(service*)realloc(run,sizeof(service)*nrun);
		run[nrun-1]=init(srv->d_name);
	}
	closedir(tolist);
	/* create list of available services */
	tolist=opendir(availdir);
	/* skip . and .. */
	readdir(tolist);
	readdir(tolist);
	while((srv=readdir(tolist))) {
		navail+=1;
		avail=(service*)realloc(avail,sizeof(service)*navail);
		avail[navail-1]=init(srv->d_name);
	}
	closedir(tolist);
	for (i=0;i<nsvcs;i+=1) {
		for (j=0;j<list[i].numdeps;j+=1) {
			/* check if services are provided */
			provided=0;
			for (k=0;k<nsvcs;k+=1) {
				if (!strcmp(list[k].name,list[i].deps[j])) {
					provided=1;
					break;
				}
			}
			if (!provided) {
				for (k=0;k<nstarted;k+=1) {
					if (!strcmp(started[k].name,list[i].deps[j])) {
						provided=1;
						break;
					}
				}
			}
			/* if not provided, find and add all needed from rundir*/
			if (!provided) {
				for (k=0;k<nrun;k+=1) {
					if (!strcmp(list[i].deps[j],run[k].name)) {
						provided=1;
						nsvcs+=1;
						list=(service*)realloc(list,nsvcs*sizeof(service));
						list[nsvcs-1]=init(run[k].name);
						break;
					}
				}
			}
			/* if nothing is in rundir, add from availdir */
			if (!provided) {
				for (k=0;k<navail;k+=1) {
					if (!strcmp(list[i].deps[j],avail[k].name)) {
						nsvcs+=1;
						list=(service*)realloc(list,nsvcs*sizeof(service));
						list[nsvcs-1]=init(avail[k].name);
						break;
					}
				}
			}
		}
	}
	free(started);
	free(run);
	free(avail);
}

int
checksorted()
{
	int i, j, k;
	for (i=0;i<nsvcs-1;i+=1) {
		if (list[i].numdeps<0) {
			for (j=i+1;j<nsvcs;j+=1) {
				if (list[j].numdeps>=0) {
					return i;
				}
			}
		} else for (j=i+1;j<nsvcs;j+=1) {
			for (k=0;k<list[i].numdeps;k+=1) {
				if (!strcmp(list[j].name,list[i].deps[k])) {
					return i;
				}
			}
		}
	}
	return nsvcs;
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
mklevels()
{
	int i, j, k;
	levels=(int*)malloc(sizeof(int));
	nlevels=1;
	for (i=0;i<nsvcs-1;i+=1) {
		for (j=i+1;j<nsvcs;j+=1) {
			for (k=0;k<list[j].numdeps;k+=1) {
				if (!strcmp(list[j].deps[k],list[i].name)) {
					i=j;
					levels[nlevels-1]=i-1;
					nlevels+=1;
					levels=(int*)realloc(levels,nlevels*sizeof(int));
					break;
					break;
				}
			}
		}
	}
	levels[nlevels-1]=nsvcs-1;
}

void
sort(int begin)
{
	int i, j, k;
	service sw;
	for (i=begin;i<nsvcs-1;i+=1) {
		if (list[i].numdeps<0) {
			for (j=nsvcs-1;j>=i;j-=1)
				if (list[j].numdeps>=0)
					break;
			sw=list[i];
			list[i]=list[j];
			list[j]=sw;
		}
		for (j=i+1;j<nsvcs;j+=1) {
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
	die("usage: %s [-skfl] [-adr directory] ...\n",name);
}
