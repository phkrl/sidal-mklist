# sidal-mklist
sidal-mklist is a tool to generate a list of services for sidal to boot

Usage:

sidal-mklist [-fks] [-adr dir] services

f - make list with all dependencies
k - make list to kill
s - make list to start
a <dir> - change path to directory with all available services. The default is /etc/sidal/avail
d <dir> - change path to directory with services to run. The default is /etc/sidal/run
r <dir> - change path to directory with started services. The default is /run/sidal
