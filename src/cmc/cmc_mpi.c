/* vi: set filetype=c.doxygen: */
#include "cmc_mpi.h"
#include "cmc.h"
#include "cmc_vars.h"

/**
* @brief Function to find start and end indices for each processor for a loop over N. Makes sure the number of elements each proc works on is an even number. Is a generic function that does not use myid, instead it should be passed through parameter i.
*
* @param N data size for which data partitioning scheme needs to be found
* @param i parameters for processor i that is needed
* @param mpiStart start index of processor i in the global dataset
* @param mpiEnd end index of processor i in the global dataset
*/
void mpiFindIndicesEvenGen( long N, int i, int* mpiStart, int* mpiEnd )
{
	N = N/2;
	int temp;
	long chunkSize = N / procs;
   if ( i < N % procs )
   {
		temp = i * chunkSize + i;
		*mpiStart = temp * 2 + 1; //+1 since for loops go from 1 to N
		*mpiEnd = ( temp + chunkSize + 1 ) * 2 + 1 - 1;
   } else {
		temp = i * chunkSize + N % procs;
		*mpiStart = temp * 2 + 1; //+1 since for loops go from 1 to N
		*mpiEnd =  ( temp + chunkSize ) * 2 + 1 - 1;
   }
}

/**
* @brief Function specially made for our code as dynamics_apply() takes 2 stars at a time. This function divides particles in pairs if the no.of stars is even, but if the no.of stars is odd, it gives one star to the last processor (since only this will be skipped by the serial code, and hence will help comparison) and divides the rest in pairs.
*
* @param N data size for which data partitioning scheme needs to be found
* @param mpiStart array for start indices
* @param mpiEnd array for end indices
*/
void mpiFindIndicesSpecial( long N, int* mpiStart, int* mpiEnd )
{
	if( N % 2 == 0 )
	{
		mpiFindIndicesEvenGen( N, myid, mpiStart, mpiEnd );
	} else {
		mpiFindIndicesEvenGen( N-1, myid, mpiStart, mpiEnd );
		if(myid == procs-1)
			*mpiEnd += 1;
	}
}

/**
* @brief Same as mpiFindIndicesSpecial, but does not use myid, instead allows it to be passed through parameter i.
*
* @param N data size for which data partitioning scheme needs to be found
* @param i parameters for processor i that is needed
* @param mpiStart start index of processor i in the global dataset
* @param mpiEnd end index of processor i in the global dataset
*/
void mpiFindIndicesSpecialGen( long N, int i, int* mpiStart, int* mpiEnd )
{
	if( N % 2 == 0 )
	{
		mpiFindIndicesEvenGen( N, i, mpiStart, mpiEnd );
	} else {
		mpiFindIndicesEvenGen( N-1, i, mpiStart, mpiEnd );
		if(i == procs-1)
			*mpiEnd += 1;
	}
}

/**
* @brief
Function to find start and end indices for the given processor i.
If blkSize is 1, N is divided equally among processors. If blkSize > 0, N*blkSize
stars are divided among procs in such a way that each contain multiples of blkSize.
*
* @param N the number of blocks of size blkSize to be divided among processors
* @param blkSize size of blocks
* @param i processor id
* @param mpiStart indices
* @param mpiEnd
*/
void mpiFindIndices( long N, int blkSize, int i, int* mpiStart, int* mpiEnd )
{
	long chunkSize =  ( N / procs ) * blkSize;
   if ( i < N % procs )
   {
      *mpiStart = i * chunkSize + i * blkSize + 1; //+1 since for loops go from 1 to N
		*mpiEnd = *mpiStart + chunkSize + 1 * blkSize - 1;
   } else {
      *mpiStart = i * chunkSize + ( N % procs ) * blkSize + 1; //+1 since for loops go from 1 to N
		*mpiEnd =  *mpiStart + chunkSize - 1;
   }
}

/**
* @brief Populates the mpiStart and mpiEnd values given the initial data size N, and the factor blkSize which data in each processor is required to be a multiple of (except the last processor). Followin is the data partitioning scheme - Each processor gets data that is a multiple of blkSize, and the remainder after division goes to the last processor. For more details, refer to: http://adsabs.harvard.edu/abs/2013ApJS..204...15P.
*
* @param N data size that needs to be partitioned
* @param blkSize the factor blkSize which data in each processor is required to be a multiple of (except the last one).
* @param mpiStart start index of the data elements in the global array belonging to processor i.
* @param mpiEnd array end index of the data elements in the global array belonging to processor i.
*/
void mpiFindIndicesCustom( long N, int blkSize, int i, int* mpiStart, int* mpiEnd )
{
	int blocks, remain;

	blocks = N / blkSize;
	remain = N % blkSize;

	mpiFindIndices( blocks, blkSize, i, mpiStart, mpiEnd );

	if(i == procs-1)
		*mpiEnd += remain;
}

/**
* @brief finds displacement and count for all processors
*
* @param N data size
* @param blkSize block size which each chunk has to be a multiple of
* @param mpiDisp displacement array
* @param mpiLen count array
*/
void mpiFindDispAndLenCustom( long N, int blkSize, int* mpiDisp, int* mpiLen )
{
	int i;
	for( i = 0; i < procs; i++ )
	{
		mpiFindIndicesCustom( N, blkSize, i, &mpiDisp[i], &mpiLen[i] );
		mpiLen[i] = mpiLen[i] - mpiDisp[i] + 1; 
	}
}


/**
* @brief Creates a new communicator with inverse order of processes.
*
* @param procs number of processors
* @param old_comm old communicator
*
* @return new communicator with inverse order of ranks
*/
MPI_Comm inv_comm_create(int procs, MPI_Comm old_comm)
{
	int i, *newranks;
	MPI_Group orig_group, new_group;
	MPI_Comm  new_comm;

	newranks = (int *) malloc (procs * sizeof(int));
	for (i=0; i<procs; i++)
		newranks[i] = procs - 1 - i;

	MPI_Comm_group(old_comm, &orig_group);
	MPI_Group_incl(orig_group, procs, newranks, &new_group);
	MPI_Comm_create(old_comm, new_group, &new_comm);

	free (newranks);
	return new_comm;
}

//Function to find start index (displacement) and length for each processor for a loop over N. Not used anymore.
/*
void mpiFindDispAndLen( long N, int* mpiDisp, int* mpiLen )
{
	long chunkSize = N / procs;
	int i;
	for( i = 0; i < procs; i++ )
	{
		if ( i < N % procs )
		{
			*mpiDisp = i * chunkSize + i + 1; //+1 since for loops go from 1 to N
			*mpiLen = chunkSize + 1;
		} else {
			*mpiDisp = i * chunkSize + N % procs + 1; //+1 since for loops go from 1 to N
			*mpiLen =  chunkSize;
		}
		mpiDisp++; //Pointer increments
		mpiLen++;
	}
}
*/

//Function to find start and end indices for each processor for a loop over N. The difference is that the number of elements each proc works on is an even number.
/*
void mpiFindIndicesEven( long N, int* mpiStart, int* mpiEnd )
{
	N = N/2;
	int temp;
	long chunkSize = N / procs;
   if ( myid < N % procs )
   {
		temp = myid * chunkSize + myid;
		*mpiStart = temp * 2 + 1; //+1 since for loops go from 1 to N
		*mpiEnd = ( temp + chunkSize + 1 ) * 2 + 1 - 1;
   } else {
		temp = myid * chunkSize + N % procs;
		*mpiStart = temp * 2 + 1; //+1 since for loops go from 1 to N
		*mpiEnd =  ( temp + chunkSize ) * 2 + 1 - 1;
   }
}
*/

/* Future Work
void mpiReduceAndBcast( void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm )
{
}
*/
