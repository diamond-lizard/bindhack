/*	
	A simple LD_PRELOAD hack to let you specify the source address
	for all outbound connections or if you want to limit a process
        to only listening on one IP

	Copyright (C) 2005 Robert J. McKay <robert@mckay.com>

	License: You can do whatever you want with it.


	Compile:

	gcc -fPIC -static -shared -o bindhack.so bindhack.c -lc -ldl

	You can add -DDEBUG to see debug output.

	Usage:

	LD_PRELOAD=/path/to/bindhack.so <command>
	
	eg:

	LD_PRELOAD=/home/rm/bindhack.so telnet example.com

	you can also specify the address to use at runtime like so:

	LD_PRELOAD=/home/rm/bindhack.so BIND_SRC=192.168.0.1 telnet example.com

*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <dlfcn.h>

#include <arpa/inet.h>

/* 
   This is the address you want to force everything to use. It can be
   overriden at runtime by specifying the BIND_SRC environment 
   variable.
*/
#define SRC_ADDR	"192.168.0.1"

/* 
   LIBC_NAME should be the name of the library that contains the real
   bind() and connect() library calls. On Linux this is libc, but on
   other OS's such as Solaris this would be the socket library
*/
#define LIBC_NAME	"libc.so.6" 

#define YES	1
#define NO	0

int
bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen)
{

	struct sockaddr_in src_addr;
	void	*libc;
	int	(*bind_ptr)(int, void *, int);
	int	ret;
	int	passthru;
	char 	*bind_src;

#ifdef DEBUG
	fprintf(stderr, "bind() override called for addr: %s\n", SRC_ADDR);
#endif

	libc = dlopen(LIBC_NAME, RTLD_LAZY);

	if (!libc)
	{
		fprintf(stderr, "Unable to open libc!\n");
		exit(-1);
	}

	*(void **) (&bind_ptr) = dlsym(libc, "bind");

	if (!bind_ptr)
	{
		fprintf(stderr, "Unable to locate bind function in lib\n");
		exit(-1);
	}
	
	passthru = YES;	/* By default, we just call regular bind() */

	if (my_addr==NULL)
	{
		/* If we get a NULL it's because we're being called
		   from the connect() hack */

		passthru = NO;

#ifdef DEBUG
		fprintf(stderr, "bind() Received NULL address.\n");
#endif

	}
	else
	{

		if (my_addr->sa_family == AF_INET)
		{
			struct sockaddr_in	myaddr_in;

			/* If this is an INET socket, then we spring to
			   action! */
			passthru = NO;

			memcpy(&myaddr_in, my_addr, addrlen);

			src_addr.sin_port = myaddr_in.sin_port;


		}
		else
		{
			passthru = YES;
		}

	}

	if (!passthru)
	{

#ifdef DEBUG
		fprintf(stderr, "Proceeding with bind hack\n");
#endif

		src_addr.sin_family = AF_INET;

		bind_src=getenv("BIND_SRC");

		/* If the environment variable BIND_SRC is set, then use
		   that as the source IP to bind instead of the hard-coded
		   SRC_ADDR one.
		*/
		if (bind_src)
		{
			ret = inet_pton(AF_INET, bind_src, &src_addr.sin_addr);
			if (ret<=0)
			{
				/* If the above failed, then try the
				   built in address. */

				inet_pton(AF_INET, SRC_ADDR, 
						&src_addr.sin_addr);
			}
		}
		else
		{
			inet_pton(AF_INET, SRC_ADDR, &src_addr.sin_addr);
		}


	/* Call real bind function */
		ret = (int)(*bind_ptr)(sockfd, 
					(void *)&src_addr, 
					sizeof(src_addr));
	}
	else
	{

#ifdef DEBUG
		fprintf(stderr, "Calling real bind unmolested\n");
#endif

	/* Call real bind function */
		ret = (int)(*bind_ptr)(sockfd, 
					(void *)my_addr, 
					addrlen);

	}
#ifdef DEBUG
	fprintf(stderr, "The real bind function returned: %d\n", ret);
#endif

	/* Clean up */
	dlclose(libc);

	return ret;

}

/* 
	Sometimes (alot of times) programs don't bother to call bind() 
	if they're just making an outgoing connection. To take care of
	these cases, we need to call bind when they call connect 
	instead. And of course, then call connect as well...
*/

int
connect(int  sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	int	(*connect_ptr)(int, void *, int);
	void	*libc;
	int	ret;

#ifdef DEBUG
	fprintf(stderr, "connect() override called for addr: %s\n", SRC_ADDR);
#endif

	/* Before we call connect, let's call bind() and make sure we're
	   using our preferred source address.
	*/

	ret = bind(sockfd, NULL, 0); /* Our fake bind doesn't really need
					those params */

	libc = dlopen(LIBC_NAME, RTLD_LAZY);

	if (!libc)
	{
		fprintf(stderr, "Unable to open libc!\n");
		exit(-1);
	}

	*(void **) (&connect_ptr) = dlsym(libc, "connect");

	if (!connect_ptr)
	{
		fprintf(stderr, "Unable to locate connect function in lib\n");
		exit(-1);
	}


	/* Call real connect function */
	ret = (int)(*connect_ptr)(sockfd, (void *)serv_addr, addrlen);

	/* Clean up */
	dlclose(libc);

	return ret;	

}



