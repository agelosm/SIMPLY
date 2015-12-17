/*Agelos Mourelis AM:2007 agelosm@csd.uoc.gr cs102007@cs.uoi.gr*/

#include "simply.h"
#include <stdio.h>
#include <netdb.h> /*for struct sockaddr_in*/
#include <stdlib.h> /*for exit()*/
#include <string.h> /*for bzero()*/
#include <assert.h>
#include <sys/types.h>
#include <sys/ipc.h> /*for shmget and shmat */
#include <sys/shm.h>
#include <errno.h>
#include <pthread.h> /*Threads used for shared memory's SIMPLY_ANY_SOURCE*/

#define machine(i) (*argv)[*argc + 3 +i]				/*place on argv where machine i process should connect is*/
#define machinesCount atoi( (*argv)[*argc -1] )				/*place on argv where the number of the available machines is*/
#define nprocs atoi( (*argv)[*argc-4 - atoi( (*argv)[*argc -1] )] )	/*place on argv where number of desired processors should be*/
#define cloneId atoi( (*argv)[*argc-3 - atoi( (*argv)[*argc -1] )] )	/*place on argv where every clone finds it's id*/
#define cport atoi( (*argv)[*argc-2 - atoi( (*argv)[*argc -1] )] )	/*place on argv where clone i's port is*/
#define noOfMems atoi( (*argv)[*argc-1] )				/*number of shared memories to be used. Exists only if shared memory is requested*/
#define BUFFER_HEAP_SIZE 10

int connectToPort(int);
int serverToPort(int);
void error(int x, char *msg);
void adjustHeap( void );
void * simply_operate( void *, enum SIMPLY_datatype, enum SIMPLY_op);
int nodesInProc(void);
void arrangeSharedMemory(int argc, char* argv[] );
int placeOfTargetInSharedMems( int target);
void * sharedSelect(void *);
void * socketSelect(void*);

/*struct that contains variables to set up a socket*/
struct sock{
	int sockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct hostent *server;
};

/*struct to store buffered messages*/
struct buffMessage{
	int sockNum;
	void *simplyBuff;

};

/*global matrix to store the available machines*/
char **machines;
int machineCount;
int serverSockDesc; /*global variable to keep up of the socket that will be closed at the end*/
fd_set mySet; /*set to be used for simply_recv's ANY_SOURCE option*/
int *rankToSocket, myId, procs, portBase;
struct buffMessage bufferHeap[BUFFER_HEAP_SIZE];
int shm = 0;	/*Flag to show wheather shared memory was requested or not*/
struct sh** sharedMems;	/*Matrix to store the share memory pointers*/
int * sharedMemsToClear;
int shSource = -1, soSource = -1;  /*Variables used for sh.mem. ANY_SOURCE*/

void simply_init(int *argc,char ***argv){
	int i=0,fd, j;
	
	
	/*Checking if -s (shared memory) is requested*/
	if( *argc > 3){
	  for(i=0; i< *argc; i++){
	    if(strcmp( (*argv)[i], "-s") == 0)
	      shm = 1; 
	  }
	}
	
	/*Check if there are shared memories passed, and adjusts arg count*/
	if(shm == 1 &&  !( *((*argv)[*argc-2]) >96 && *((*argv)[*argc-2]) <123 ) )
	  *argc -= noOfMems + 1;
	  
	/*allocating memory for the machine matrix*/
	machines = malloc(machinesCount*sizeof(char*) );
	for(i = 0; i < machinesCount; i++)
		machines[i] = malloc(20*sizeof(char) );

	rankToSocket = malloc(nprocs*sizeof(int)); /*Allocating matrix to store the sockets, e.g clone 2 in rankToSocket[2]*/

	/*initializing the heap*/
	for(i=0 ; i<BUFFER_HEAP_SIZE; i++)
		bufferHeap[i].sockNum = 0;
	myId = cloneId;	/*copying arguments to global variables for later use*/
	procs = nprocs;
	machineCount = machinesCount; /*keeping number of available machines*/
	portBase = cport; /* setting the main port, so process with id x will connect to portBase+x*/
	
	if( procs <= machineCount)
		shm = 0;
	
	
	/*Allocating memory to store the shared memory parts*/
	if( shm == 1 && procs>machineCount){
	  sharedMems = malloc( ( nodesInProc() ) * sizeof(struct sh*));
	  for(i=0; i<nodesInProc(); i++){
	    sharedMems[i] = malloc(sizeof(struct sh*));
	  }
	  sharedMemsToClear = malloc( (nodesInProc())*sizeof(int));
	}
	/*Case where the process will generate the remaining processes on the same CPU, passing them the share memory adresses*/
	if( shm == 1 && myId < machineCount && procs > machineCount)
	  arrangeSharedMemory(*argc,*argv);
	else if( shm == 1 && myId >= machineCount){	/*Case where process was generated, and stores shared memories that were passed*/
	  for(i=0; i<nodesInProc()-1; i++){
	    sharedMems[i] = (struct sh *) shmat( atoi( (*argv)[*argc+i]), 0, 0);
	    sharedMemsToClear[i] = atoi( (*argv)[*argc+i]);
	  }
	}

	


	*argc -= 4+machineCount; /*adjusting argcount to be correct for the user after init was called*/
	
	/*initializing table with machines*/
	for(i=0; i<machineCount; i++)
		strcpy(machines[i],machine(i));
	
	/*Clone servers himself at proper port*/	
	serverSockDesc = serverToPort(portBase+myId);

	/*setting up the procs*(procs-1)/2 sockets*/
        for(i=procs; i > 1; i--){
	   if(myId == i%procs ){ 
	    for(j = 1; j < i; j++){
	      
		   /*If share memory was requested, and processes belong to the same CPU,nothing happens, else socket connection will be done*/
		   if( !( shm == 1 && j%machineCount == myId%machineCount ) ){
		    fd = connectToPort( portBase +j);
		    rankToSocket[j] = fd;
		   }
		   else
		     rankToSocket[j] = 0;
	    }
	   }
	    else if( myId < i && myId > 0 ){
	      
	      if( !( shm == 1 && (i%procs)%machineCount == myId%machineCount ) ){
		fd = acceptClient( serverSockDesc );
		rankToSocket[i%procs] = fd;
	      }
	      else
		rankToSocket[i%procs] = 0;
	    }
	 	
	  }

	rankToSocket[ myId ] = 0;	/*Changing our own port with 0*/
	(*argv)[*argc] = NULL;		/*NULL pointer for the new environment*/
	return;
}	

void simply_finalize(){

	int i;
  
	error(close(serverSockDesc),"closing"); /*close the socket where process was servered on*/
	
	free(rankToSocket);

	for(i = 0; i < machineCount; i++)
		free(machines[i]);
	free(machines);
	
	/*Free shared memory*/
	if( shm == 1 && procs>machineCount){
	  for(i=0; i<nodesInProc()-1;i++)
	      shmctl(sharedMemsToClear[i], IPC_RMID, 0);
	}
	
	
	  
}

int simply_send(void *buf, int count,enum SIMPLY_datatype dtype, int dest, int tag){
	
	int  mySize = simply_size(dtype);
	int i;	

	/*allocating memory to store the tag followed by the message*/
	void *tempBuf = malloc( (count)*mySize + sizeof(int) );

	/* placing tag as header*/
	*(int *)tempBuf = tag;

	/* move the pointer just after the tag*/
	for( i=0; i<sizeof(int); i++)
		(char *)tempBuf ++;

	bcopy(buf, tempBuf, count*mySize);

	for(i=0; i<sizeof(int); i++)
		(char *)tempBuf --;
	
	/*send the adjusted message*/	
	
	if( shm == 1 && dest%machineCount == myId%machineCount )
	  contactViaShared(dest, tag, buf, count*mySize);
	else
	  error(send(rankToSocket[dest],tempBuf,count*mySize+sizeof(int),0),"sending");
	
	return 1;
}
void simply_comm_size(int *npr){
	*npr = procs;
}

void simply_comm_rank(int *myrank){
	*myrank = myId;
}

int simply_recv(void *buf, int count,enum SIMPLY_datatype dtype, int source, int tag, struct SIMPLY_status *status){

	int i, mySize = simply_size(dtype), tempHeapSize;
	int myTag, flag = 0;
	void *tempBuf = malloc( (count)*mySize + sizeof(int));
	pthread_t threadIds[2];

	/*checking the buffer if a message with matching tag has allready arrived*/
	for(i=0; i< heapSize(); i++){
		if(  (bufferHeap[i].sockNum == source || source < 0)&& ( *(int *)(bufferHeap[i].simplyBuff) == tag || tag == SIMPLY_ANY_TAG ) 
){
			
			/*if a message was found, copy it and remove it from the heap*/
			bcopy(bufferHeap[i].simplyBuff, tempBuf, count*mySize + sizeof(int) );
			free(bufferHeap[i].simplyBuff);
			bufferHeap[i].sockNum = 0;
			adjustHeap();
			flag = 1;
 			break;
		}
	}
	
      
      if(source < 0 ){
	if( shm == 0)
	  source = (int)socketSelect(NULL);	/*if request is ANY_SOURCE, using select to decide where the message came from*/
	else if(shm == 1){
	  pthread_create( &threadIds[0], NULL, socketSelect, NULL);	/*Thread to call select and wait for a socket response*/
	  pthread_create( &threadIds[1], NULL, sharedSelect, NULL);	/*Thread to wait for a shared memory message*/
	  
	  while( shSource < 0 && soSource < 0)	/*Block until one of the global variables is updated*/
	    usleep(1);
	
	  if( shSource >= 0){
	    source = shSource;			/*Update source*/
	    pthread_cancel(threadIds[0]);	/*Kill the other thread*/
	    soSource = -1;			/*Set global variables to default*/	
	    shSource = -1;
	  }
	  else if( soSource >= 0){
	    source = soSource;
	    pthread_cancel(threadIds[1]);
	    soSource = -1;
	    shSource = -1;
	  }

        }        
      }
	/*Filling inputed status with the source found*/
	if(status != NULL)
	  (*status).source = source;
				
	/*if a message wasnt found, receive one*/
	while(!flag){

		/*If share memory was requested, and processes are in the same CPU, receiving is done from the shared memory*/
		if( shm == 1 && source%machineCount == myId%machineCount){

		  if( source < myId){
		    while( sharedMems[ placeOfTargetInSharedMems(source) ]->newMessage1 != 1)
		     usleep(1);
		    bcopy( sharedMems[ placeOfTargetInSharedMems(source) ]->buffer1, tempBuf, count*mySize);
		    myTag = sharedMems[ placeOfTargetInSharedMems(source) ]->tag1;
		    sharedMems[ placeOfTargetInSharedMems(source) ]->newMessage1 = 0;
		  }
		  if( source > myId){
		   while( sharedMems[ placeOfTargetInSharedMems(source) ]->newMessage2 != 1)
		     usleep(1);
		   bcopy( sharedMems[ placeOfTargetInSharedMems(source) ]->buffer2, tempBuf, count*mySize);
		   myTag = sharedMems[ placeOfTargetInSharedMems(source) ]->tag2;
		   sharedMems[ placeOfTargetInSharedMems(source) ]->newMessage2 = 0;;
		  }
		
		}
		else{ /*Receiving is done through the socket*/
		  error(recv( rankToSocket[source],tempBuf,(count)*mySize + sizeof(int),0),"receiving");
		  myTag = (*(int *)tempBuf)%100;
		}		

		/*Filling inputed status with the tag received*/
		if(status != NULL)
		  (*status).tag = myTag;
		  
		if( tag == myTag || tag == SIMPLY_ANY_TAG)
			break;
		else if( myTag != tag && tag != SIMPLY_ANY_TAG){	/* If tags dont match, store message to the buffers*/
			tempHeapSize = heapSize();
			if(tempHeapSize > BUFFER_HEAP_SIZE){
				fprintf(stderr,"Out of buffer\n");
				exit(0);
			}
			/*Buffering*/
			bufferHeap[ tempHeapSize ].simplyBuff = malloc( count*mySize + sizeof(int) );
			bcopy(tempBuf, bufferHeap[ tempHeapSize ].simplyBuff, count*mySize + sizeof(int) );
			bufferHeap[ tempHeapSize ].sockNum = source;
		}
	}
	/*If message came through a socket, ignore the tag and copy the rest*/
	if( !(shm == 1 && source%machineCount == myId%machineCount) ){
	  for(i=0; i<sizeof(int); i++)
		  (char *)tempBuf++;
	}
    
	  
	bcopy(tempBuf, buf, (count)*mySize);;
	
	return 1;
}

int simply_barrier(void){

	int j;
	int tempBuf = 313;	/*Temporary buffer to store the barrier messages*/

	/*Every process will receive a message using a tree algorithm*/
	for(j=1; j<procs; j*=2){
			if( myId < j && myId+j <= procs-1)
				simply_send(&tempBuf, 1, SIMPLY_INT, myId+j,2); 
			else if( myId < 2*j && myId >= j)
				simply_recv(&tempBuf,1,SIMPLY_INT, myId-j,2 , NULL);
	}
return 1;
}

int simply_bcast(void *sbuf, int count,enum SIMPLY_datatype dtype, int rootrank){

	int overlap1 = 0, myNewId, overlap2 = 0, j;

	/*Finding out if an overlap occurs*/
	if(myId - rootrank < 0 )
		overlap1 = 1;

	/*Calculating the process' new id, so rootrank will have new id 0, etc*/
	myNewId = myId + (procs*overlap1) - rootrank;

	/*Broadcasting the message using a tree algorithm*/
	for(j=1; j<procs; j*=2){
			if(myId - j < 0 )
				overlap2 = 1;

			if( myNewId < j && myNewId+j <= procs-1)
				simply_send(sbuf,count,dtype, (myId+j)%procs,3);
			else if( myNewId < 2*j && myNewId >= j)
				simply_recv(sbuf, count, dtype,myId - j + overlap2*procs,3, NULL); 
	}
return 1;

}

int simply_scatter(void *sendbuf, int sendcnt,enum SIMPLY_datatype sendtype, void *recvbuf, int recvcnt,enum SIMPLY_datatype recvtype, int root){

	int i, j;

	if(sendcnt == 0 || recvcnt == 0 )
		return 0;

	/*Root divides and sends the inputed buffer*/
	if( myId == root){
		for(i=0; i<procs; i++){
			if(myId != i)
				simply_send(sendbuf, sendcnt, sendtype, i, 4);
			else
				bcopy(sendbuf, recvbuf, recvcnt*simply_size(recvtype) );
					
			for(j=0; j < sendcnt*simply_size(sendtype); j++)
				(int *)sendbuf++;

		}
	}
	else
		simply_recv( recvbuf, recvcnt, recvtype, root, 4, NULL);     /*All other processes receive their part*/

return 1;
}

int simply_gather(void *sendbuf, int sendcnt,enum SIMPLY_datatype sendtype, void *recvbuf, int recvcnt,enum SIMPLY_datatype recvtype, int root){

	int i, j;

	if(sendcnt == 0 || recvcnt == 0 )
		return 0;

	/*Root receives each part and places it correctly on inputed buffer*/
	if( myId == root){
		for(i=0; i<procs; i++){
			if(myId != i)
				simply_recv( recvbuf, recvcnt, recvtype, i, 5, NULL);
			else
				bcopy(sendbuf, recvbuf, recvcnt*simply_size(recvtype) );
			for(j=0; j < recvcnt*simply_size(sendtype); j++)
				(int *)recvbuf++;

		}
	}
	else
		simply_send(sendbuf, sendcnt, sendtype, root, 5); /*All other processes send their part*/

return 1;
}

int simply_allgather(void *sendbuf, int sendcnt,enum SIMPLY_datatype sendtype, void *recvbuf, int recvcnt,enum SIMPLY_datatype recvtype){
	
	int i;

	if(sendcnt == 0 || recvcnt == 0 )
		return 0;

	/* 0 gathers*/
	simply_gather(sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, 0);	

	/*0 sends it to all others, with linear complexity*/
	for(i=1; i<procs; i++){
		if(myId == 0)
			simply_send(sendbuf, procs*sendcnt, sendtype, i, 6);
		else if(myId == i)
			error(recv( rankToSocket[0],recvbuf,procs*recvcnt*simply_size(recvtype),0),"receiving");
			simply_recv( recvbuf, procs*recvcnt, recvtype, 0, 6, NULL);
	}

return 1;
}

int simply_reduce ( void *sendbuf, void *recvbuf, int count, enum SIMPLY_datatype dtype,enum SIMPLY_op op, int root){

	int i, j;

	/*Buffer to store elements received from all processes*/
	void *tempBuf;

	if(count == 0 )
		return 0;
	
	if( myId == root)
	    tempBuf = malloc(count*simply_size(dtype) );

	/*Root receives all parts and stores them on tempBuf*/
	if( myId == root){
		for(i=0; i<procs; i++){
			if(myId != i)
				simply_recv( tempBuf, count, dtype, i, 7, NULL);
			else
				bcopy(sendbuf, tempBuf, count*simply_size(dtype) );

			for(j=0; j < count*simply_size(dtype); j++)
				(int *)tempBuf++;


		}
	}
	else
		simply_send(sendbuf, count, dtype, root, 7);
	
	/*Changing the tempBuf pointer to the initial one*/
	if(myId == root){
	  for(i=0; i<procs*count*simply_size(dtype); i++)
	    (char *)tempBuf --;
	
	  /*Getting the result from operate function and copies it*/
	  tempBuf = simply_operate(tempBuf, dtype, op);
	  
	  bcopy( tempBuf, recvbuf, count*simply_size(dtype) );
	  
	}
	
	if(myId == root)
	  free(tempBuf);
	

return 1;
}
  
  



int serverToPort(int port){

	struct sock s;

	/*Opening the socket*/
	error(s.sockfd = socket(AF_INET,SOCK_STREAM,0),"opening socket");	
	
	/*Emptying ser_addr struct's bytes*/
	bzero( (char *)&(s.serv_addr), sizeof(s.serv_addr) );

	/*setting struct serv addr's parameters*/
       (s.serv_addr).sin_family = AF_INET;
       (s.serv_addr).sin_addr.s_addr = INADDR_ANY;
       (s.serv_addr).sin_port = htons(port);
       
	/*binding sockfd socket and checking for errors*/
       	error(bind(s.sockfd, (struct sockaddr *) &(s.serv_addr),sizeof(s.serv_addr)), "binding");

	/*listening up to #(argument 2) clients on sockfd port*/
	listen(s.sockfd,procs);
	
	/*returns the socket descriptor*/
	return s.sockfd;

}

int connectToPort(int port){
	
	struct sock s;
	error(s.sockfd = socket(AF_INET, SOCK_STREAM, 0), "opening socket");

	/*setting name to localhost since the process is forked*/
	s.server = gethostbyname(machines[ (port-portBase)%(machineCount)]);
	if (s.server == NULL) {
		printf("ERROR, no such host\n");
	     	exit(0);
	   }
	/*Emptying serv_addr struct's bytes*/
	bzero((char *) &(s.serv_addr), sizeof(s.serv_addr));
	/*setting struct's parameters(should agree with servers)*/
	(s.serv_addr).sin_family = AF_INET;
   	bcopy((char *)(s.server)->h_addr,(char *)&(s.serv_addr).sin_addr.s_addr,(s.server)->h_length);

	(s.serv_addr).sin_port = htons(port);
	/*make connect to block until the other process starts listening*/
	while(connect(s.sockfd,(struct sockaddr *) &(s.serv_addr),sizeof(s.serv_addr)) < 0);
	FD_SET(s.sockfd, &mySet);
	return s.sockfd;
}

int acceptClient(int socket_desc){

    int addrlen,new_socket;
    struct sockaddr_in address;
    addrlen = sizeof(struct sockaddr_in);
    error(new_socket = accept(socket_desc, (struct sockaddr *)&address, (socklen_t *)&addrlen),"accepting");
    FD_SET(new_socket,&mySet);

    /*return the descriptor returned by accept*/
    return new_socket;
}

/*assisting function to check for errors, and print during what task they occured*/
void error(int x, char *msg){
	if(x<0){
		fprintf(stderr,"ID %d: Error while %s\n",myId,msg);
		fprintf(stderr, "ID %d: Error cause is: %s\n",myId, strerror( errno ) );
		close(serverSockDesc);
		exit(0);
	} 
}

/*Function to specify the size of given datatype*/
int simply_size(enum SIMPLY_datatype dtype){
  	switch( dtype ) 
	{
	  case SIMPLY_INT:
	    return sizeof(int);
	  case SIMPLY_CHAR:
	    return sizeof(char);
	  case SIMPLY_DOUBLE:
	    return sizeof(double);
	  default :
	    return 0;
	}
}

/*Function to return the current size of the heap*/
int heapSize( void ){
	int i = 0;
	while( bufferHeap[i].sockNum != 0){
	
		if( i == BUFFER_HEAP_SIZE)
			break;
		i++;

	}
	return i;
}

/*Function to swap heap elements, so the search of heapSize function will be correct*/
void adjustHeap( void ){

	int i,j;

	for(i=0 ; i< BUFFER_HEAP_SIZE; i++){
		if( bufferHeap[i].sockNum != 0)
			for(j=0; j < i;j++){
				if( bufferHeap[j].sockNum == 0){
					bufferHeap[j].simplyBuff = bufferHeap[i].simplyBuff;
					bufferHeap[i].simplyBuff = NULL;
	
					bufferHeap[j].sockNum = bufferHeap[i].sockNum;
					bufferHeap[i].sockNum = 0;

					break;
				}
			}
	}
}

/*Function to help reducing, operates inputed operation to inputed buffer*/
void * simply_operate( void *buffer, enum SIMPLY_datatype dtype, enum SIMPLY_op op){
  
  if(op == SIMPLY_SUM){
    int i;
    void *sum;
    
    if( dtype == SIMPLY_DOUBLE ){
      
      sum = malloc(sizeof(double));
      
      *(double *)sum = 0.0;
      
      for(i=0; i<procs; i++)
	*(double *)sum += ((double *)buffer)[i];
	
      return sum;
    }
    else if( dtype == SIMPLY_INT){
      
      sum = malloc(sizeof(int));
      
      *(int *)sum = 0;
      for(i=0; i<procs; i++)
	*(int *)sum += ((int *)buffer)[i];
      
      return sum;
    }
    else
      return NULL;
    
  }
  

}

/*Function that calculates how many processes will exist in the current CPU*/
int nodesInProc( void){
  
  int count = 0, x = myId%machineCount;
  
  while( x < procs){
    x += machineCount;
    count ++;
  }
  return count;
}

/*Assisting function that calculates the sum from 'from' to 'to'*/
int sum(int from, int to){
  int i, count = 0;
  for(i=from; i <= to; i++)
    count += i;
  
  return count;
}

/*Function to arrange the shared memory, pass it to other processes and call the generating fucntion*/
void arrangeSharedMemory(int argc, char *argv[] ){
  
  int i, j, k, memid, clone, posInProc;
  int shMemsNeeded = ( nodesInProc()* (nodesInProc() - 1) )/2;
  int mems[shMemsNeeded];
  int hisSh[nodesInProc()-1]; /*Matrix to store shared mem adresses to be passed in the process*/
  
  for(i=0; i< shMemsNeeded; i++){
     error(memid = shmget(IPC_PRIVATE, sizeof(struct sh), 0600|IPC_CREAT),"getting the shared memory id");
     mems[i] = memid;
  }
  
  for(i=0; i<nodesInProc()-1; i++){
      sharedMems[i] = (struct sh *) shmat( mems[i], 0, 0);
      
      if( sharedMems[i] == (void *)(-1) )
	error((int)(sharedMems[i]),"allocating shared memory");

	sharedMems[i]->newMessage1 = 0;
	sharedMems[i]->newMessage2 = 0;


  }
  
  /*Generating clone (i+1)*machineCount + (myId%machineCount) at each part of the loop*/
  for( i=0; i < nodesInProc()-1; i++){
    
    clone = (i+1)*machineCount + (myId%machineCount);
    posInProc = clone/machineCount;
    
    j = 0;
    for( j=0; j < posInProc; j++)
	hisSh[j] = mems[ j*nodesInProc() - sum(0,j) + posInProc -1 -j];
    
    
    for( k=0; k < nodesInProc() - 1 - posInProc; k++)
      hisSh[ j+k] = mems[ posInProc*nodesInProc() - sum(0,posInProc) +k];

      /*calling the generating function, that will create the process with id 'clone'*/
      generateClone(clone, machineCount, portBase, argc, machines, procs, argv, hisSh );
  }
  

  
}

/*Process that sets up an environment and creates a process*/
void generateClone(int id, int machineCount, int port, int argc, char **machines, int npr, char *argv[], int *shmat){
	int j,pid1, shm = 0;	
	char **temp;
	
	if( shmat != NULL)
	  shm = 1;
	
	if( !shm){
	  temp = malloc( (argc+5+machineCount)*sizeof(char*));
	  for(j=0; j<argc+6+machineCount; j++)
		temp[j] = malloc(30*sizeof(char));
	}
	else{
	  temp = malloc( (argc + nodesInProc() )*sizeof(char*) );
	  for(j=0; j<argc+nodesInProc(); j++)
		temp[j] = malloc(50*sizeof(char));
	}

	if(id != npr-1 || shm == 1)
		pid1 = fork();
	else
		pid1 = 0;
	
	
	
	
      if( pid1 == 0 && shm == 1){	/*Case where shared mem adresses will be passed: call was made through init function*/
		
		/*copying current process' arguments*/
		for(j=0; j<argc; j++)
		    strcpy(temp[j],argv[j]);
		
		/*Replacing the one that contains the process id*/
		sprintf(temp[ argc - 3 - atoi(argv[argc-1]) ], "%d", id);
		
		/*Adding shared memory adresses*/
		for(j=0; j<nodesInProc()-1; j++)
		  sprintf(temp[argc+j], "%d", shmat[j]);
		
		/*Adding number of shared memories passed*/
		sprintf(temp[argc+nodesInProc()-1], "%d", nodesInProc()-1);
		
		/*NULL pointer for the new environment*/
		temp[argc+nodesInProc()] = NULL;

		error( execvp(argv[0],temp), "exevping");

      }
      else if(pid1 == 0 && shm == 0){	/*Case where there is no shared memory: call was made through simplyrun*/
		temp[0] = "ssh";
		temp[1] = "-n";
		/*machine to ssh to , %machineCount in case i > machineCount*/
		temp[2] = machines[id%machineCount];
		temp[3] = argv[2];
		/*copying user arguments*/
		for(j=0; j<argc-3; j++){
			strcpy(temp[j+4],argv[j+3]);
		}
		/*copying port, id, and nprocs*/
		temp[argc+1] = argv[1];
		sprintf(temp[argc+2],"%d",id);
		sprintf(temp[argc+3],"%d",port);
		/*copying all the machines, and how many are they*/
		for(j=0; j<machineCount; j++)
			strcpy(temp[argc+4+j],machines[j]);
		sprintf(temp[argc+4+machineCount],"%d",machineCount);
		temp[argc+5+machineCount] = NULL;
		
		error( execvp("/usr/bin/ssh",temp), "execvping");
	}

		return ;

}

/*Function that calculates the place on shared memory matrix, that current process shared memory with inputed process*/
int placeOfTargetInSharedMems( int target){
  
  int targetMatrixPos;
  
  if( target > myId )
    targetMatrixPos = (target/machineCount) - 1;
  else
    targetMatrixPos = (target/machineCount);
  
  return targetMatrixPos;
}

/*Function that sends a message throughout shared memory, updating the buffer, the tag and the new message flag*/
int contactViaShared(int target, int tag, void *buffer, int size){
  
  if(target > myId){
    while( sharedMems[ placeOfTargetInSharedMems(target) ]->newMessage1 == 1)
      usleep(1);
    bcopy( buffer, sharedMems[ placeOfTargetInSharedMems( target )]->buffer1, size);
    sharedMems[ placeOfTargetInSharedMems( target )]->tag1 = tag; 
    sharedMems[ placeOfTargetInSharedMems( target )]->newMessage1 = 1;
  }
  else{
    while( sharedMems[ placeOfTargetInSharedMems(target) ]->newMessage2 == 1)
      usleep(1);
    sharedMems[ placeOfTargetInSharedMems( target )]->tag2 = tag;
    bcopy(buffer, sharedMems[ placeOfTargetInSharedMems( target )]->buffer2, size);
    sharedMems[ placeOfTargetInSharedMems( target )]->newMessage2 = 1;
    
  }
  
  return 1;
}

/*Function that falls in a lopp that ends when a new message is received through shared memory (simulation of select socket function)*/
void * sharedSelect( void *x){
  
   int i; 
   
   while(1){
     for(i=myId%machineCount; i<procs; i += machineCount ){
       
         
       if( i == myId);       
       else if(myId > i ){
	 if( sharedMems[ placeOfTargetInSharedMems(i)]->newMessage1 == 1){
	   shSource = i;
	   pthread_exit(NULL);

	 }
       }
       else{
	 if( sharedMems[ placeOfTargetInSharedMems(i)]->newMessage2 == 1){
	   shSource = i;
	   pthread_exit(NULL);

	 }
       }
     }
   }
     return (void *)(-1);     
}

/*Function that uses select, to decide who's contacting the current process through a socket*/
void * socketSelect( void *x){
  
  fd_set theirSet;
  int i;
  
  FD_ZERO(&theirSet);
  theirSet = mySet; 
  error(select(procs+5,&theirSet,NULL,NULL,NULL),"selecting");
  for(i=0; i<procs; i++){
    if(FD_ISSET(rankToSocket[i],&theirSet)){
      soSource = i;
      if( shm == 1)
	pthread_exit(NULL);
      else if(shm == 0)
	return (void *)i;

    }
  } 
  return (void *)(-1);
}




