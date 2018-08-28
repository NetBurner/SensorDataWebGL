#NB_COPYRIGHT#

#This will build NAME.x and save it as $( NBROOT ) / bin / NAME.x
NAME    = WebGL
CXXSRCS := main.cpp FileSystemUtils.cpp htmldata.cpp web.cpp ftp_f.cpp

#Uncomment and modify these lines if you have C or S files.
#CSRCS : = foo.c
#ASRCS : = foo.s
CREATEDTARGS := htmldata.cpp

XTRALIB := $(NBROOT)/lib/WebClient.a

#include the file that does all of the automagic work!
include $(NBROOT)/make/main.mak


htmldata.cpp : $( wildcard html/*.*)
	comphtml html -ohtmldata.cpp
