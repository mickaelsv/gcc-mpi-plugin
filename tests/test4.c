#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#pragma Projet_CA mpicoll_check (main, main, banane)
#pragma Projet_CA mpicoll_check test1, test2
#pragma Projet_CA mpicoll_check (test3 test4)
#pragma Projet_CA mpicoll_check (test5, test6
#pragma Projet_CA mpicoll_check test7)
#pragma Projet_CA mpicoll_check ,
#pragma Projet_CA mpicoll_check (,
#pragma Projet_CA mpicoll_check (test8,)

int main(int argc, char * argv[])
{
	#pragma Projet_CA mpicoll_check main
	MPI_Init(&argc, &argv);

	MPI_Finalize();
	return 1;
}
