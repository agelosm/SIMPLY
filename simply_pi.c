#include "simply.h"
#include <stdio.h>
#include <sys/times.h>
#include <unistd.h>

#define N 1000000000

main(int argc, char *argv[]){
	double  w, result = 0.0, temp;
	int i,myid,nproc;

	struct SIMPLY_status myStatus;

	struct tms a;
	int t1,t2,t3,t4;
	
	t1 = times(&a);
	simply_init(&argc,&argv);
	simply_comm_rank(&myid);
	simply_comm_size(&nproc);

	t2 = times(&a);
	
	w = 1.0/((double)N);
	
	for(i=myid; i<N; i+=nproc)
		result += 4*w/(1+(i+0.5)*(i+0.5)*w*w);
	t2 = times(&a);

	simply_reduce(&result,&result,1,SIMPLY_DOUBLE, SIMPLY_SUM,0);
	
		
	if(myid == 0)
		printf("pi = %.45lf\n", result);

	t3 = times(&a);
	simply_barrier();

	simply_finalize();

	t4 = times(&a);
	

	if(myid == 0)
	{
		printf("Total time: %lf\n", (double)(t4-t1)/sysconf(_SC_CLK_TCK) );
		printf("Time for communications: %lf\n", (double)(t3-t2)/sysconf(_SC_CLK_TCK) );
		printf("Time for init: %lf\n", (double)(t2-t1)/sysconf(_SC_CLK_TCK) );
	}
}
