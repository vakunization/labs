#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void forvarding(int fd);
void read_requesthdrs(rio_t *rp);
int parse_url(char *url, char *host_hdr, char *request, char *port);
void request_to_target(int fg, char *request, char* host);

void *thread(void *vargp);

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, *connfdp;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2){
    printf("usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    //printf("accepted connection from (%s, %s)\n", hostname, port);
    Pthread_create(&tid, NULL, thread, connfdp);
  }
  return 0;
}

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  forvarding(connfd);
  Close(connfd);
  return NULL;
}

void forvarding(int fd){
  char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
  rio_t rio;
  rio_t s_rio;
  
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;

  sscanf(buf, "%s %s %s", method, url, version);
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not Implemented", "Proxy does not implement this method");
    return;
  }

  read_requesthdrs(&rio);
  
  char host_hdr[MAXLINE];
  char request[MAXLINE];
  char port[MAXLINE];
    
  if (parse_url(url+1, host_hdr, request, port)){
    int target_fd;
    if (strlen(port) != 0)
      target_fd = Open_clientfd(host_hdr, port);
    else
      target_fd = Open_clientfd(host_hdr, "80");
    
    if (target_fd == -1){
      clienterror(fd, host_hdr, "502", "Bad Gateway", "");
      return;
    }
    
    request_to_target(target_fd, request, host_hdr);

    size_t n;
    Rio_readinitb(&s_rio, target_fd);
    while ((n = Rio_readnb(&s_rio, buf, MAXLINE)) > 0){
      Rio_writen(fd, buf, n);
    }
    
    Close(target_fd);
  } else
    clienterror(fd, request, "400", "Bad Request", "");

  return;
}

int parse_url(char *url, char *host, char *request, char *port){
  char *ptr_service, *ptr_port, *ptr_request;
  size_t len = 0;
  ptr_service = strstr(url, "://");
  if (ptr_service)
    url = ptr_service + 3;
  
  ptr_request = strstr(url, "/");
  if (ptr_request){
    strcpy(request, ptr_request);
  } else
    return 0;
  
  ptr_port = strstr(url, ":");
  if (ptr_port){
    len = ptr_port - url;
    strncpy(host, url, len);
    
    ptr_port += 1;
    len = ptr_request - ptr_port;
    strncpy(port, ptr_port, len);
  } else {
    len = ptr_request - url;
    strncpy(host, url, len);
  }

  if (strlen(host) == 0)
    return 0;
  
  return 1;
}

void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];
    do {
    Rio_readlineb(rp, buf, MAXLINE);
    //printf("%s", buf);
    }  while(strcmp(buf, "\r\n"));
    return;
}

void request_to_target(int fd, char *request, char *host) {
  char buf[MAXLINE];

  sprintf(buf, "GET %s HTTP/1.0\r\n", request);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Host: %s\r\n", host);
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, (char *)user_agent_hdr, strlen(user_agent_hdr));
  Rio_writen(fd, "Connection: close\r\n", 19);
  Rio_writen(fd, "Proxy-Connection: close\r\n\r\n", 27);
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
}
