/*Agelos Mourelis AM:2007 agelosm@csd.uoc.gr cs102007@cs.uoi.gr*/

#include <stdio.h>
#include <malloc.h>	/*for malloc()*/
#include <string.h>
#include <assert.h>


#define execName argv[2]
#define npr atoi(argv[1])


int main(int argc, char *argv[]){

	int i, shm = 0;	
	
	srand(time(NULL));	
	int port = 1024+(rand()%10000);
	
	char **machines;

	int machineCount = makeMachines(&machines);

	if(argc>3 ){
		for( i = 0; i<argc; i++){
			if( strcmp(argv[i],"-s") == 0  && npr > machineCount)
				shm = 1;
		}
	}
	
	if( shm == 0){
		for(i=0; i<npr; i++)
			generateClone(i, machineCount, port, argc, machines, npr, argv, NULL);
	}
	else{
		for(i=0; i<machineCount;i++)
			generateClone(i, machineCount, port, argc, machines, npr, argv, NULL);
	}

	return 0;
}

 





/*Function to read from file mymachines and allocate and initialize a table including them*/
int makeMachines(char ***machines ){

	int machineCount = 0, i;
	char temp[20];
	FILE *fp;

	fp = fopen("mymachines","r");

	while( fscanf(fp,"%s",temp) != EOF)
		machineCount ++;

	fclose(fp);

	*machines = malloc( (machineCount+1) * sizeof(char*) );

	fp = fopen("mymachines","r");

	for(i=0; i<machineCount; i++){
		(*machines)[i] = malloc(45*sizeof(char));
		fscanf(fp,"%s",temp);
		strcpy((*machines)[i],temp);
	}
	fclose(fp);
	
	return machineCount;
}
