//цель: выдать сортированный список скриптов по зависимостям. Добавить опции для выделения зависимых и независимых служб
#include "utils.h"
#include <unistd.h>

#define START 1<<0
#define STOP 1<<1

typedef struct
{
	char *name;
	char **deps;
	int numdeps;
	/* aliases for service (like udev or nldev should provide dev) */
	char **provide;
	int numprovide;
} service;

static int checksorted(service *list, int n);
static void sort(service *list, int n, int begin);
static void usage(char *name);

int mode=0;

int
main(int argc, char *argv[])
{
	char **svcs=(char**)malloc(0), *dir="/etc/sidal/run";
	char buf[256];
	service *list;
	int nsvcs=0, i, j, k;
	FILE *pipe;
	for(i=1;i<argc;i+=1) {
		if (argv[i][0]=='-') {
			if (!strcmp(argv[i],"--help") || strchr(argv[i],'h')) {
				usage(argv[0]);
			} else if (strchr(argv[i],'s') || !strcmp(argv[i],"--start")) {
				mode=(mode&!STOP)|START;
			} else if (strchr(argv[i],'k') || !strcmp(argv[i],"--stop")) {
				mode=(mode&!START)|STOP;
			} else if (strchr(argv[i],'d')) {
				dir=argv[++i];
			} else {
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
		list[i].name=svcs[i];
	}
	
	for(i=0;i<nsvcs;i+=1) {
		/* get info about dependencies */
		if (!(pipe=fopen(smprintf("%s/%s",dir,svcs[i]),"r"))) {
			/* if these services are not in /etc/sidal/run, mark them to run in the end */
			list[i].deps=0;
			list[i].numdeps=-1;
			continue;
		} else {
			fclose(pipe);
			pipe=popen(smprintf("%s/%s depend",dir,svcs[i]),"r");
			list[i].deps=(char**)malloc(0);
			list[i].numdeps=0;
		}
		while(fscanf(pipe,"%s",buf)!=EOF) {
			list[i].deps=(char**)realloc(list[i].deps,(list[i].numdeps+1)*sizeof(char*));
			list[i].deps[list[i].numdeps]=smprintf("%s",buf);
			list[i].numdeps+=1;
		}
		pclose(pipe);
		/* get aliases */
		if (!(pipe=popen(smprintf("%s/%s provide",dir,svcs[i]),"r"))) {
			list[i].provide=0;
			list[i].numprovide=-1;
			continue;
		} else {
			list[i].provide=(char**)malloc(0);
			list[i].numprovide=0;
		}
		while(fscanf(pipe,"%s",buf)!=EOF) {
			list[i].provide=(char**)realloc(list[i].provide,(list[i].numprovide+1)*sizeof(char*));
			list[i].provide[list[i].numprovide]=smprintf("%s",buf);
			list[i].numprovide+=1;
		}
		pclose(pipe);
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

int
checksorted(service *list, int n)
{
	int i, j, k, l;
	for (i=0;i<n-1;i+=1)
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
				for (l=0;l<list[j].numprovide;l+=1)
					if (!strcmp(list[j].provide[l],list[i].deps[k])) {
						return i;
				}
			}
		}
	return n;
}

void
sort(service *list, int n, int begin)
{
	int i, j, k, l;
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
				} else for (l=0;l<list[j].numprovide;l+=1) {
					if (!strcmp(list[j].provide[l],list[i].deps[k])) {
						sw=list[i];
						list[i]=list[j];
						list[j]=sw;
					}
				}
			}
		}
	}
}

void
usage(char *name)
{
	die("usage: %s [-sk] [-d directory] ...\n",name);
}
