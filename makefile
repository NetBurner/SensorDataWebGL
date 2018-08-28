# Copyright 1998-2018 NetBurner, Inc.  ALL RIGHTS RESERVED
#
#    Permission is hereby granted to purchasers of NetBurner Hardware to use or
#    modify this computer program for any use as long as the resultant program
#    is only executed on NetBurner provided hardware.
#
#    No other rights to use this program or its derivatives in part or in
#    whole are granted.
#
#    It may be possible to license this or other NetBurner software for use on
#    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
#
#    NetBurner makes no representation or warranties with respect to the
#    performance of this computer program, and specifically disclaims any
#    responsibility for any damages, special or consequential, connected with
#    the use of this program.
#
# NetBurner
# 5405 Morehouse Dr.
# San Diego, CA 92121
# www.netburner.com

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
