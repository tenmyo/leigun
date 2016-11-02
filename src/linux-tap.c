/*
 *************************************************************************************************
 *
 * Linux Ethernet tunneling using /dev/net/tun
 * Used from Network interface Emulators
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#ifdef __linux__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <asm/types.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <linux/if_tun.h>
#include <configfile.h>


static void
exec_ifconfig(const char *ifname,const char *ipaddr) 
{ 
	pid_t pid;
	int status;
	if(!(pid=fork())) {
		execlp("sudo","/sbin/ifconfig","/sbin/ifconfig",ifname,ipaddr,NULL);
		fprintf(stderr,"exec sudo ifconfig failed\n");
		exit(13424);
	}
	waitpid(pid,&status,0);
	if(status!=0)  {
		fprintf(stderr,"ifconfig failed with status %d\n",status);
	}
}

static void
bridge_addif(const char *ifname,const char *bridge) 
{ 
	pid_t pid;
	int status;
	if(!(pid=fork())) {
		execlp("sudo","/sbin/brctl","/sbin/brctl","addif",bridge,ifname,NULL);
		fprintf(stderr,"exec sudo brctl failed\n");
		exit(13774);
	}
	waitpid(pid,&status,0);
	if(status!=0)  {
		fprintf(stderr,"brctl failed with status %d\n",status);
	}
}

static int 
configure_interface(const char *devname,const char *ifname,int ifnr) 
{
	char *ipaddr;
	char *bridge;
	ipaddr=Config_ReadVar(devname,"host_ip");	
	bridge=Config_ReadVar(devname,"bridge");
	if(!ipaddr && !bridge) {
		fprintf(stderr,"No IP-Address and no bridge given for Interface %s(%d)\n",ifname,ifnr);
		return 0;
	}
	if(ipaddr && strlen(ipaddr)) {
		exec_ifconfig(ifname,ipaddr);
	} 
	if(bridge && strlen(bridge)) {
		bridge_addif(ifname,bridge);
	}
	return 0;
}

/*
 * -------------------------------------
 * Create an ethernet tunnel 
 * -------------------------------------
 */

static int 
tap_open(const char *name)
{
    struct ifreq ifr;
    int fd;
    int i;
    int status;
    char cmdbuf[1024];
    if(!name) {
	return -1;
    }
    sprintf(cmdbuf,"sudo sg_tunctl -t %s -u %d",name,getuid());
    status = system(cmdbuf);
    if(status != 0) {
	fprintf(stderr,"Can not create network TAP: sg_tunctl error %d: %s\n",
		WEXITSTATUS(status),strerror(WEXITSTATUS(status)));	 
	sleep(1);
    }
    /* 
     **************************************************************
     * udev needs some time to configure the permissions of
     * /dev/net/tun when the tun.ko module 
     **************************************************************
     */
    for(i = 20; i > 0; i--) {
	    if( (fd = open("/dev/net/tun", O_RDWR)) >= 0 ) {
			break;
	    }
	    if(errno == EPERM) {
		fprintf(stderr,"Retry opening /dev/net tun because of permission\n");
	    	usleep(100000);
	    }
    }
    if(i == 0) {
	perror("Can not open /dev/net/tun");
	sleep(1);
	return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
	if(errno != EBUSY) {
	 	perror("ioctl TUNSETIFF");
         	goto failed;
	}
    }
    return fd;
failed:
    sleep(1);
    fprintf(stderr,"Ethernet Tunnel creation failed for \"%s\"\n",name);
    close(fd);
    return -1;
}

static int ifcounter=0;

int 
Net_CreateInterface(const char *devname) {
	int fd;
	int result;
	char *ifname;
	ifname=Config_ReadVar(devname,"host_ifname");	
	if(!ifname) {
		fprintf(stderr,"No host interface name in configfile for interface %s(%d)\n",devname,ifcounter);
		return -1;
	}
	fd=tap_open(ifname);
	if(fd<0)
		return fd;
	result=configure_interface(devname,ifname,ifcounter);
	ifcounter++;
	if(result<0) { 
		return result;
	}
	return fd;
}

#ifdef TAPTEST
int
main() {
	int i;
	tap_create("tap0");
	exec_ifconfig("tap0");
	while(1) {

	}
}
#endif

#endif
