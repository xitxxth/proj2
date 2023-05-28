/* 
 * echoservert_pre.c - A prethreaded concurrent echo server
 */
/* $begin echoservertpremain */
#include "csapp.h"
#define NTHREADS  4
#define SBUFSIZE  16
#define MAX_STOCK 129
#define MAX_CHARACTERS 100
#ifndef __SBUF_H__
#define __SBUF_H__

typedef struct {
    int *buf;          /* Buffer array */         
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
/* $end sbuft */

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

#endif /* __SBUF_H__ */

void echo_cnt(int connfd);
void *thread(void *vargp);

sbuf_t sbuf; /* Shared buffer of connected descriptors */
static int byte_cnt;  /* Byte counter */
static sem_t mutex;   /* and the mutex that protects it */

FILE* fp; //FILE OPEN POINTER
struct item{
	int ID; // stock ID
	int left_stock;// the number of left stocks
	int price; // price of stock
	int readcnt; // ?
	struct item* left_child;
	struct item* right_child;
}; //stock node by project2.pdf
struct item* stock_tree; //manage heap as array
/*user defined function*/
void stock_tree_init(void);//initialize stock list
void show_stock(int);//show stock list
void buy_stock(int, int, int);//buy stock
void sell_stock(int, int, int);//sell stock
void parse_cmd(char *, int*);//parse command line
void insert_heap(int, int, int);
void SIGINT_HANDLER(int s);
struct item* search_tree(int); 

int main(int argc, char **argv) 
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid; 

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

	Signal(SIGINT, SIGINT_HANDLER);
	stock_tree_init();
	fp = fopen("stock.txt", "r");
	if(!fp){
		printf("FILE OPEN ERROR\n");
		exit(0);
	}
	// int tmp_id, tmp_price, tmp_left;// temporary variable for input
	// int i=0;
	// while(fscanf(fp, "%d %d %d", &tmp_id, &tmp_left, &tmp_price)!=EOF){
	// 	stock_tree[i].ID = tmp_id;
	// 	stock_tree[i].left_stock = tmp_left;
	// 	stock_tree[i].price = tmp_price;
	// 	//update stock list
	// 	i++;//update index
	// }//
	int tmp_id, tmp_left, tmp_price;
	while(fscanf(fp, "%d %d %d", &tmp_id, &tmp_left, &tmp_price)!=EOF)	{
		insert_heap(tmp_id, tmp_left, tmp_price);
		}
	
	fclose(fp);//close file pointer

    sbuf_init(&sbuf, SBUFSIZE); //line:conc:pre:initsbuf
    for (i = 0; i < NTHREADS; i++)  /* Create worker threads */ //line:conc:pre:begincreate
	    Pthread_create(&tid, NULL, thread, NULL);               //line:conc:pre:endcreate

    while (1) { 
	printf("WAITING!\n");
    clientlen = sizeof(struct sockaddr_storage);
	connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
	sbuf_insert(&sbuf, connfd); /* Insert connfd in buffer */
    }
}

void *thread(void *vargp) 
{  
    Pthread_detach(pthread_self()); 
    while (1) { 
	printf("THREAD ON!\n");
	int connfd = sbuf_remove(&sbuf); /* Remove connfd from buffer */ //line:conc:pre:removeconnfd
	echo_cnt(connfd);                /* Service client */
	Close(connfd);
    }
}
/* $end echoservertpremain */
/* 
 * A thread-safe version of echo that counts the total number
 * of bytes received from clients.
 */
/* $begin echo_cnt */
static void init_echo_cnt(void)
{
    Sem_init(&mutex, 0, 1);
    byte_cnt = 0;
}

void echo_cnt(int connfd) 
{
    int n; 
    char buf[MAXLINE]; 
    char cmdline[MAXLINE];
    rio_t rio;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    int parsed_ans[2];

    Pthread_once(&once, init_echo_cnt); //line:conc:pre:pthreadonce
    Rio_readinitb(&rio, connfd);        //line:conc:pre:rioinitb
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        strcpy(cmdline, buf);
        if(strncmp(cmdline, "show", 4)==0)	show_stock(connfd);
		else if(strncmp(cmdline, "buy", 3)==0){
			parse_cmd(cmdline, parsed_ans);
			buy_stock(parsed_ans[0], parsed_ans[1], connfd);
		}
		else if(strncmp(cmdline, "sell", 3)==0){
			parse_cmd(cmdline, parsed_ans);
			sell_stock(parsed_ans[0], parsed_ans[1], connfd);
		}
		else if(strncmp(cmdline, "exit", 4)==0){
			printf("client dead\n");
			Close(connfd); //line:conc:echoservers:closeconnfd
		}
        else    break;
	}
	    /* EOF detected, remove descriptor from pool */
	// P(&mutex);
	// byte_cnt += n; //line:conc:pre:cntaccess1
	// printf("server received %d (%d total) bytes on fd %d\n", 
	//        n, byte_cnt, connfd); //line:conc:pre:cntaccess2
	// V(&mutex);
	// Rio_writen(connfd, buf, n);
    return;
}
/* $end echo_cnt */
/*user defined functions*/
void stock_tree_init(void)
{
	stock_tree = NULL;
}

void show_stock(int connfd)
{
	if(stock_tree==NULL)	return;
	char cat_list[MAXLINE];
	for(int i=0; cat_list[i]; i++)	cat_list[i] = '\0';
	char tmp_str[MAX_CHARACTERS];
	struct item* stack[MAX_STOCK];
	struct item* curr = stock_tree;
	int top=-1;
	//printf("before: %s\n", cat_list);
	P(&mutex);
	while(curr!=NULL || top !=-1) {
		while(curr != NULL){
			stack[++top] = curr;
			curr = curr->left_child;
		}
		curr = stack[top--];
		sprintf(tmp_str, "%d %d %d\n", curr->ID, curr->left_stock, curr->price);
		strcat(cat_list, tmp_str);
		curr = curr->right_child;
	}
	V(&mutex);
	//printf("after: %s\n", cat_list);
	Rio_writen(connfd, cat_list, sizeof(cat_list));
}

void buy_stock(int id, int quant, int connfd)
{
	char buf[MAXLINE];
	struct item* curr;
	P(&mutex);
	if(!(curr  = search_tree(id))){
		V(&mutex);
		strcpy(buf, "Wrong ID\n");
		Rio_writen(connfd, buf, sizeof(buf));
		return;
	}
	if(quant > curr->left_stock){
		V(&mutex);
		strcpy(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	else{
		curr->left_stock -= quant;
		V(&mutex);
		strcpy(buf, "[buy] success\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	return;
}

void sell_stock(int id, int quant, int connfd)
{
	char buf[MAXLINE];
	struct item* curr;
	P(&mutex);
	if(!(curr  = search_tree(id))){
		V(&mutex);
		strcpy(buf, "Wrong ID\n");
		Rio_writen(connfd, buf, sizeof(buf));
		return;
	}
	curr->left_stock += quant;
	V(&mutex);
	strcpy(buf, "[sell] success\n");
	Rio_writen(connfd, buf, sizeof(buf));
	return;
}

void parse_cmd(char* cmd, int * parsed_ans)
{
	int i, j, k;
	char string_id[10], string_quant[10];
	for(i=0; cmd[i]!=' '; i++){ }
	for(j=i+1; cmd[j]!=' '; j++){ }
	strncpy(string_id, cmd + i + 1, j-i-1);
	for(k=j+1; cmd[k]; k++){ }
	strncpy(string_quant, cmd + j + 1, k-j-1);
	parsed_ans[0] = atoi(string_id);
	parsed_ans[1] = atoi(string_quant);
	return;
}

void SIGINT_HANDLER(int s)
{
	int olderrno = errno;
	fp = fopen("stock.txt", "w");
	struct item* stack[MAX_STOCK];
	struct item* curr = stock_tree;
	int top=-1;
	while(1) {
		while(curr != NULL){
			stack[++top] = curr;
			curr = curr->left_child;
		}
		if(top>=0){
			curr = stack[top--];
			fprintf(fp, "%d %d %d\n", curr->ID, curr->left_stock, curr->price);
			curr = curr->right_child;
		}
		else{
			break;
		}
	}
	fclose(fp);
    errno = olderrno;
}

void insert_heap(int tmp_id, int tmp_left, int tmp_price)
{
	struct item* new_stock;
	new_stock = (struct item*)malloc(sizeof(struct item));
	new_stock->ID = tmp_id;
	new_stock->left_stock = tmp_left;
	new_stock->price = tmp_price;
	new_stock->left_child = NULL;
	new_stock->right_child = NULL;
	struct item* curr;
	struct item* prev;
	if(stock_tree==NULL){
		stock_tree = new_stock;
		return;
	}
	curr = stock_tree;
	while(1){
		prev = curr;
			if(tmp_id < curr->ID){
				curr=curr->left_child;
				if(curr==NULL){
					prev->left_child = new_stock;
					return;
				}
			}
			else{
				curr=curr->right_child;
				if(curr==NULL){
					prev->right_child = new_stock;
					return;
				}
			}
	}
}

struct item* search_tree(int id) {
    struct item* curr = stock_tree;
	P(&mutex);
    while (curr != NULL) {
        if (id == curr->ID) {
			V(&mutex);
            return curr;
        } else if (id < curr->ID) {
            curr = curr->left_child;
        } else {
            curr = curr->right_child;
        }
    }
	V(&mutex);
    return NULL;
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
/* $begin sbuf_init */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
/* $end sbuf_init */

/* Clean up buffer sp */
/* $begin sbuf_deinit */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
/* $end sbuf_deinit */

/* Insert item onto the rear of shared buffer sp */
/* $begin sbuf_insert */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
/* $end sbuf_insert */

/* Remove and return the first item from buffer sp */
/* $begin sbuf_remove */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
/* $end sbuf_remove */
/* $end sbufc */