/* This is F5 Graceful Scaling helper daemon.
 *
 * Sami Kerola <sami.kerola@sabre.com>
 */

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define STATE_FILE	"f5gs.conf"
#define BUF_SIZE             1024
#define PORT_NUM            32546	/* f5 == in octal */
#define PEND_CONNECTIONS      128	/* pending connections to hold */

unsigned int client_s;
long pagesz;
void *message;

/* child thread */
void *response_thread(void *arg)
{
	char in_buf[BUF_SIZE];
	ssize_t retcode;

	/* let the client send, and ignore */
	retcode = recv(client_s, in_buf, BUF_SIZE, 0);

	if (retcode < 0)
		printf("recv error\n");
	else
		send(client_s, message, pagesz, 0);
	close(client_s);
	pthread_exit(NULL);
	/* should be impossible to reach */
	return 0;
}

int main(int argc, char **argv)
{
	socklen_t server_s;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t addr_len;
	unsigned int ids;
	pthread_attr_t attr;
	pthread_t threads;
	int fd;

	server_s = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr)))
		err(EXIT_FAILURE, "unable to bind");
	if (listen(server_s, PEND_CONNECTIONS))
		err(EXIT_FAILURE, "unable to listen");

	pagesz = sysconf(_SC_PAGESIZE);
	fd = open(STATE_FILE, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, "cannot open file: %s", STATE_FILE);
	message = mmap(NULL, pagesz, PROT_READ, MAP_SHARED, fd, 0);

	pthread_attr_init(&attr);
	while (1) {
		addr_len = sizeof(client_addr);
		client_s = accept(server_s, (struct sockaddr *)&client_addr, &addr_len);

		if (client_s < 0)
			err(EXIT_FAILURE, "unable to create socket");
		else {
			ids = client_s;
			pthread_create(&threads, &attr, response_thread, &ids);

		}
	}

	munmap(message, pagesz);
	close(server_s);
	return EXIT_SUCCESS;
}
