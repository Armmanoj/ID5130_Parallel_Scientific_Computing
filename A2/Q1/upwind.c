#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#define dx 0.002
#define dt 0.001
#define c 1
#define L 2
#define T 1
#define fileout "out.txt"

double* upwind(int width, MPI_Comm comm, int rank, int size){
    int N = L/dx+1;
    int M = T/dt+1;
    double* u = (double*)malloc(M*width*sizeof(double));
    // now setting initial conditions
    for (int i=0; i<width;i++){
        if (((width-1)*rank+i)<0.5/dx){
            u[i] = sin(4*M_PI*((width-1)*rank+i)*dx);
        }
        else{
            u[i] = 0;
        }
    }
    for (int i = width; i<width*M;i++){
        u[i] = 0;
    }
    MPI_Barrier(comm);
    double sol_speed = c*dt/dx;
    for(int t=0;t<M-1;t+=1){
        // Computation
        if (rank < size - 1) {
            for(int i=0; i<width-1;i++){
                u[i+width*(t+1)+1] = u[i+width*t+1]-sol_speed*(u[i+width*t+1]-u[i+width*t]);
            }
        }
        else{
            for(int i=0; i<N-(size-1)*(width-1)-1;i++){
                u[i+width*(t+1)+1] = u[i+width*t+1]-sol_speed*(u[i+width*t+1]-u[i+width*t]);
            }
        }
        // Communication
        if (rank < size - 1) {
            MPI_Send(u+width*(t+2) - 1, 1, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD);
        }
        if (rank > 0) {
            MPI_Recv(u+width*(t+1), 1, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    return u;
}

int main(int argc, char** argv){
    MPI_Init(&argc, &argv);
    FILE* file;
    int size, rank;
    int width;
    MPI_Status status;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int N = L/dx+1;
    int M = T/dt+1;
    
    if (N%size==0){
    width = N/size+1; // +1 ghost point
    }
    else{
        width = N/size+2; // +1 ghost point, extra points with final processor, that are not evaluated for
    }
    // creating receive array
    double* recv_array;
    if (rank==0){
        recv_array = (double*)malloc(sizeof(double)*size*(width-1));
    }
    else{
        recv_array=NULL;
    }

    // solving the matrix
    double* u = upwind(width, MPI_COMM_WORLD,rank,size);
    MPI_Barrier( MPI_COMM_WORLD);
    // next receive the solutions for t=0,0.5,1.0
    for (int j =0; j<M;j+=(M-1)/2){

        MPI_Gather(u+j*width, width-1, MPI_DOUBLE, recv_array, width-1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (rank==0){
            printf("\n%d\n",j);
            file = fopen(fileout, "a"); // Open the file in append mode
            if (file == NULL) {
                printf("Error opening file %s for appending.\n", fileout);
                return -1;
            }

            // Write 1st N array elements to the file, separated by space
            for (int i = 0; i < N; i++) {
                fprintf(file, "%f ", recv_array[i]); 
            }
            fprintf(file, "\n"); // Add newline at the end

            fclose(file); // Close the file
        }
    }
    free(u);
    if (rank==0){
        free(recv_array);
    }
    MPI_Finalize();
    return 0;
}