/* 
 * echoservers.c - A concurrent echo server based on select
 */
/* $begin echoserversmain */
#include "csapp.h"
/*user defined macro*/
#include <stdlib.h>
#define MAX_STOCK 100
#define MAX_CHARACTERS 100
typedef struct { /* Represents a pool of connected descriptors */ //line:conc:echoservers:beginpool
    int maxfd;        /* Largest descriptor in read_set */   
    fd_set read_set;  /* Set of all active descriptors */
    fd_set ready_set; /* Subset of descriptors ready for reading  */
    int nready;       /* Number of ready descriptors from select */   
    int maxi;         /* Highwater index into client array */
    int clientfd[FD_SETSIZE];    /* Set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* Set of active read buffers */
} pool; //line:conc:echoservers:endpool
/* $end echoserversmain */
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);
void SIGINT_HANDLER(int s);
/* $begin echoserversmain */

int byte_cnt = 0; /* Counts total bytes received by server */
/* user defined data structure*/
FILE* fp; //FILE OPEN POINTER
struct item{
	int ID; // stock ID
	int left_stock;// the number of left stocks
	int price; // price of stock
	int readcnt; // ?
	sem_t mutex; // mutex lock
}; //stock node by project2.pdf
struct item stock_tree[MAX_STOCK]; //manage heap as array
/*user defined function*/
void stock_tree_init(void);//initialize stock list
void show_stock(int);//show stock list
void buy_stock(int, int, int);//buy stock
void sell_stock(int, int, int);//sell stock
void parse_cmd(char *, int*);//parse command line

int main(int argc, char **argv)
{
	Signal(SIGINT, SIGINT_HANDLER);
	stock_tree_init();
	fp = fopen("stock.txt", "r");
	if(!fp){
		printf("FILE OPEN ERROR\n");
		exit(0);
	}
	int tmp_id, tmp_price, tmp_left;// temporary variable for input
	int i=0;
	while(fscanf(fp, "%d %d %d", &tmp_id, &tmp_left, &tmp_price)!=EOF){
		stock_tree[i].ID = tmp_id;
		stock_tree[i].left_stock = tmp_left;
		stock_tree[i].price = tmp_price;
		//update stock list
		i++;//update index
	}//
	fclose(fp);//close file pointer
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool); //line:conc:echoservers:initpool

    while (1) {
	/* Wait for listening/connected descriptor(s) to become ready */
	pool.ready_set = pool.read_set;
	pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

	/* If listening descriptor ready, add new client to pool */
	if (FD_ISSET(listenfd, &pool.ready_set)) { //line:conc:echoservers:listenfdready
        clientlen = sizeof(struct sockaddr_storage);
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:conc:echoservers:accept
	    add_client(connfd, &pool); //line:conc:echoservers:addclient
	}
	
	/* Echo a text line from each ready connected descriptor */ 
	check_clients(&pool); //line:conc:echoservers:checkclients
    }

}
/* $end echoserversmain */

/* $begin init_pool */
void init_pool(int listenfd, pool *p) 
{
    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;                   //line:conc:echoservers:beginempty
    for (i=0; i< FD_SETSIZE; i++)  
	p->clientfd[i] = -1;        //line:conc:echoservers:endempty

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;            //line:conc:echoservers:begininit
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set); //line:conc:echoservers:endinit
}
/* $end init_pool */

/* $begin add_client */
void add_client(int connfd, pool *p) 
{
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
	if (p->clientfd[i] < 0) { 
	    /* Add connected descriptor to the pool */
	    p->clientfd[i] = connfd;                 //line:conc:echoservers:beginaddclient
	    Rio_readinitb(&p->clientrio[i], connfd); //line:conc:echoservers:endaddclient

	    /* Add the descriptor to descriptor set */
	    FD_SET(connfd, &p->read_set); //line:conc:echoservers:addconnfd

	    /* Update max descriptor and pool highwater mark */
	    if (connfd > p->maxfd) //line:conc:echoservers:beginmaxfd
		p->maxfd = connfd; //line:conc:echoservers:endmaxfd
	    if (i > p->maxi)       //line:conc:echoservers:beginmaxi
		p->maxi = i;       //line:conc:echoservers:endmaxi
	    break;
	}
    if (i == FD_SETSIZE) /* Couldn't find an empty slot */
	app_error("add_client error: Too many clients");
}
/* $end add_client */

/* $begin check_clients */
void check_clients(pool *p) 
{
    int i, connfd, n;
    char buf[MAXLINE]; 
	char cmdline[MAXLINE];
    rio_t rio;
	/*user defined data structure*/
	int parsed_ans[2];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
	connfd = p->clientfd[i];
	rio = p->clientrio[i];

	/* If the descriptor is ready, echo a text line from it */
	if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) { 
	    p->nready--;
	    if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		byte_cnt += n; //line:conc:echoservers:beginecho
		printf("Server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);
		strcpy(cmdline, buf);
		//Rio_writen(connfd, buf, n); //line:conc:echoservers:endecho
		if(strncmp(cmdline, "show", 4)==0)	show_stock(connfd);
		else if(strncmp(cmdline, "buy", 3)==0){
			parse_cmd(cmdline, parsed_ans);
			buy_stock(parsed_ans[0], parsed_ans[1], connfd);
		}
		else if(strncmp(cmdline, "sell", 3)==0){
			parse_cmd(cmdline, parsed_ans);
			sell_stock(parsed_ans[0], parsed_ans[1], connfd);
		}
		else if(strncmp(cmdline, "exit", 4)==0){ }
		else	printf("WRONG REQUEST\n");		
	    }

	    /* EOF detected, remove descriptor from pool */
	    else { 
		Close(connfd); //line:conc:echoservers:closeconnfd
		FD_CLR(connfd, &p->read_set); //line:conc:echoservers:beginremove
		p->clientfd[i] = -1;          //line:conc:echoservers:endremove
	    }
	}
    }
}
/* $end check_clients */
/*user defined functions*/
void stock_tree_init(void)
{
	for(int i=0; i<MAX_STOCK; i++){
		stock_tree[i].ID = -1;
		stock_tree[i].left_stock = -1;
		//stock_tree[i].mutex = -1;
		stock_tree[i].price = -1;
		stock_tree[i].readcnt = -1;
	}
}

void show_stock(int connfd)
{
	char cat_list[MAXLINE];
	char tmp[MAX_CHARACTERS];
	for(int i=0; stock_tree[i].ID>0; i++){
		sprintf(tmp, "%d %d %d\n", stock_tree[i].ID, stock_tree[i].left_stock, stock_tree[i].price);
		strcat(cat_list, tmp);
	}
	Rio_writen(connfd, cat_list, sizeof(cat_list));
}

void buy_stock(int id, int quant, int connfd)
{
	int i;
	char buf[MAXLINE];
	for(i=0; stock_tree[i].ID != id; i++){} // sotck_tree[i].ID == id
	if(quant > stock_tree[i].left_stock){
		strcpy(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	else{
		stock_tree[i].left_stock -= quant;
		strcpy(buf, "[buy] success\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	return;
}

void sell_stock(int id, int quant, int connfd)
{
	int i;
	char buf[MAXLINE];
	for(i=0; stock_tree[i].ID != id; i++){} // sotck_tree[i].ID == id
	stock_tree[i].left_stock += quant;
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
	for(int i=0; stock_tree[i].ID>0; i++)	fprintf(fp, "%d %d %d\n", stock_tree[i].ID, stock_tree[i].left_stock, stock_tree[i].price);
	fclose(fp);
    errno = olderrno;
}