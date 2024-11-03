#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#pragma Projet_CA mpicoll_check (main, mpi_invalid, mpi_valid)

void mpi_not_checked() {
	MPI_Barrier(MPI_COMM_WORLD);	
}

void mpi_invalid(int c) {
	if (c > 5) {
		MPI_Barrier(MPI_COMM_WORLD);
		return;
	} else MPI_Barrier(MPI_COMM_WORLD);
	
	MPI_Barrier(MPI_COMM_WORLD);
	return;
}

void mpi_valid(int c) {
        if (c > 5) {
                MPI_Barrier(MPI_COMM_WORLD);
        } else MPI_Barrier(MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        return;
}

int main(int argc, char * argv[])
{
  MPI_Init(&argc, &argv);
  int c = 5;
  if (c < 20) {
    if (c < 10)
    {
      MPI_Barrier(MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return 1;
}
