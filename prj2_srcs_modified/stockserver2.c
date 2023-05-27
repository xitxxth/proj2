sbuf_t sbuf; /* Shared buffer of connected descriptors */
int main(int argc, char **argv) {
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE); 
    for ( i = 0; i < NTHREADS; i++ ) /* Create a pool of worker threads */
        Pthread_create(&tid, NULL, thread, NULL); 
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
}

void *thread(void *vargp) {
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf); /* Remove connfd from buf */
        echo_cnt(connfd); /* Service client */
        Close(connfd);
    }
}

static int byte_cnt; /* Byte counter */
static sem_t mutex; /* and the mutex that protects it */

static void init_echo_cnt(void)
{
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void echo_cnt(int connfd)
{
    int n;
    char buf[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    Pthread_once(&once, init_echo_cnt); 
    Rio_readinitb(&rio, connfd); 
    while ((n = Rio_readlineb(&rio, buf, MAXLINE))!=0) {
    
    P(&mutex);
    byte_cnt += n; 
    printf("thread %d received %d (%d total) bytes on fd %d\n", (int) pthread_self(), n, byte_cnt, connfd); 
    V(&mutex);
    
    Rio_writen(connfd, buf, n);
}
} 