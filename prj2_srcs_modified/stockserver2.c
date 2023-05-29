/* 
 * echoservers.c - A concurrent echo server based on select
 */
/* $begin echoserversmain */
#include "csapp.h"
/*user defined macro*/
#include <stdlib.h>
#define MAX_STOCK 129
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
struct item* search_tree(int); 

int main(int argc, char **argv)
{
		Signal(SIGINT, SIGINT_HANDLER);
	stock_tree_init();
	fp = fopen("stock.txt", "r");
	if(!fp){
		printf("FILE OPEN ERROR\n");
		exit(0);//OPEN STOCK.TXT
	}
	int tmp_id, tmp_left, tmp_price;
	while(fscanf(fp, "%d %d %d", &tmp_id, &tmp_left, &tmp_price)!=EOF)	{
		insert_heap(tmp_id, tmp_left, tmp_price);//INSERT DATA
	}
	
	
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

	int j=1;
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
		strcpy(cmdline, buf);//COPY BUF TO CMDLINE
		//Rio_writen(connfd, buf, n); //line:conc:echoservers:endecho
	    if(strncmp(cmdline, "show", 4)==0)	show_stock(connfd);//SHOW
		else if(strncmp(cmdline, "buy", 3)==0){//BUY %D %D
			parse_cmd(cmdline, parsed_ans);//PARSE %D %D
			buy_stock(parsed_ans[0], parsed_ans[1], connfd);//BUY
		}
		else if(strncmp(cmdline, "sell", 3)==0){//SELL
			parse_cmd(cmdline, parsed_ans);//SELL %D %D
			sell_stock(parsed_ans[0], parsed_ans[1], connfd);//SELL
		}
	    }
	    /* EOF detected, remove descriptor from pool */
	    else {
		//printf("client dead\n");
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
	stock_tree = NULL;//POINT NULL
}

void show_stock(int connfd)
{
	if(stock_tree==NULL)	return;//EMPTY TREE
	char cat_list[MAXLINE];//FINAL RESULT
	for(int i=0; cat_list[i]; i++)	cat_list[i] = '\0';//KILL STATIC DATA
	char tmp_str[MAX_CHARACTERS];//TEMPORARY STRING
	struct item* stack[MAX_STOCK];//STACK FOR TRAVERSE TREE
	struct item* curr = stock_tree;//PTR
	int top=-1;//STACK TOP
	//printf("before: %s\n", cat_list);
	while(curr!=NULL || top !=-1) {
		while(curr != NULL){
			stack[++top] = curr;
			curr = curr->left_child;
		}
		curr = stack[top--];//TRAVERSE
		sprintf(tmp_str, "%d %d %d\n", curr->ID, curr->left_stock, curr->price);//PUT IN STRING
		strcat(cat_list, tmp_str);//CONCATENATE STRINGS
		curr = curr->right_child;//TRAVERSE
	}
	//printf("after: %s\n", cat_list);
	Rio_writen(connfd, cat_list, sizeof(cat_list));//SEND RESULT
}

void buy_stock(int id, int quant, int connfd)
{
	char buf[MAXLINE];//STRING LINE
	struct item* curr;
	if(!(curr  = search_tree(id))){//UNFOUND
		strcpy(buf, "Wrong ID\n");
		Rio_writen(connfd, buf, sizeof(buf));
		return;
	}
	if(quant > curr->left_stock){//NOT ENOUGH STOCKS
		strcpy(buf, "Not enough left stock\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	else{
		curr->left_stock -= quant;//SUCCESS!
		strcpy(buf, "[buy] success\n");
		Rio_writen(connfd, buf, sizeof(buf));
	}
	return;
}

void sell_stock(int id, int quant, int connfd)
{
	char buf[MAXLINE];
	struct item* curr;
	if(!(curr  = search_tree(id))){//UNFOUND
		strcpy(buf, "Wrong ID\n");
		Rio_writen(connfd, buf, sizeof(buf));
		return;
	}
	curr->left_stock += quant;//SUCCESS!
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
	strncpy(string_id, cmd + i + 1, j-i-1);//"%D" %D
	for(k=j+1; cmd[k]; k++){ }
	strncpy(string_quant, cmd + j + 1, k-j-1);// %D "%D"
	parsed_ans[0] = atoi(string_id);//CHANGE INTO INTEGER "%D" %D
	parsed_ans[1] = atoi(string_quant);//CHANGE INTO INTEGER %D "%D"
	return;
}

void SIGINT_HANDLER(int s)
{
	int olderrno = errno;
	fp = fopen("stock.txt", "w");//OPEN FOR WRITE
	struct item* stack[MAX_STOCK];//TRAVERSE STACK
	struct item* curr = stock_tree;//PTR
	int top=-1;//STACK TOP
	while(1) {
		while(curr != NULL){
			stack[++top] = curr;
			curr = curr->left_child;//TRAVERSE
		}
		if(top>=0){
			curr = stack[top--];//TRAVERSE
			fprintf(fp, "%d %d %d\n", curr->ID, curr->left_stock, curr->price);//SAVE IT
			curr = curr->right_child;//TRAVERSE
		}
		else{
			break;
		}
	}
	fclose(fp);
    errno = olderrno;
	exit(0);//END
}

void insert_heap(int tmp_id, int tmp_left, int tmp_price)
{
	struct item* new_stock;//NEW STOCK NODE
	new_stock = (struct item*)malloc(sizeof(struct item));
	new_stock->ID = tmp_id;
	new_stock->left_stock = tmp_left;
	new_stock->price = tmp_price;
	new_stock->left_child = NULL;
	new_stock->right_child = NULL;//SET
	struct item* curr;
	struct item* prev;
	if(stock_tree==NULL){
		stock_tree = new_stock;
		return;//EMTPY? SAVE IT
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
	}//TRAVERSING AND FIND EMPTY PLACE FOR NEW NODE
}

struct item* search_tree(int id) {
    struct item* curr = stock_tree;
    while (curr != NULL) {//FINDING NODE HAS ID "id"
        if (id == curr->ID) {
            return curr;
        } else if (id < curr->ID) {
            curr = curr->left_child;
        } else {
            curr = curr->right_child;
        }
    }
    return NULL;
}