#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

#pragma Projet_CA mpicoll_check (main, main, test)

int main(int argc, char * argv[])
{
  MPI_Init(&argc, &argv);

  int a = 2, b = 3, c = 0;

  if (c < 10)
  {
    if (c < 5)
    {
      a = a * a +1;
      MPI_Barrier(MPI_COMM_WORLD);
    }
    else
    {
      a = a * 3;
      MPI_Barrier(MPI_COMM_WORLD);
    }

    c += a * 2;
  }
  else
  {
    b = b * 4;
    MPI_Barrier(MPI_COMM_WORLD);
  }

  c += a + b;

  printf("c=%d\n", c);

  MPI_Finalize();
  return 1;
}
