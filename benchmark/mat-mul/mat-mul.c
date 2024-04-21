#include <stdio.h>
#include <stdlib.h>

#define SIZE 100  // Define the size of the matrix

// Function to allocate memory for a matrix
double** create_matrix() {
    double** matrix = (double**) malloc(SIZE * sizeof(double*));
    for (int i = 0; i < SIZE; i++) {
        matrix[i] = (double*) malloc(SIZE * sizeof(double));
    }
    return matrix;
}

// Function to free memory of a matrix
void free_matrix(double** matrix) {
    for (int i = 0; i < SIZE; i++) {
        free(matrix[i]);
    }
    free(matrix);
}

// Function to read matrix from file
void read_matrix(const char* filename, double** matrix) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (fscanf(file, "%lf", &matrix[i][j]) != 1) {
                fprintf(stderr, "Error reading matrix from file.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    fclose(file);
}

// Function to write matrix to file
void write_matrix(const char* filename, double** matrix) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            fprintf(file, "%.2f ", matrix[i][j]);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}


// Function to multiply two matrices
void multiply_matrices(double** A, double** B, double** C) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            C[i][j] = 0;  // Initialize the element to 0
            for (int k = 0; k < SIZE; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main() {
    // Allocate memory for matrices A, B, and C
    double **A = create_matrix();
    double **B = create_matrix();
    double **C = create_matrix();

    // Read matrices A and B from files
    read_matrix("A.txt", A);
    read_matrix("B.txt", B);

    // Multiply matrices A and B, result in C
    multiply_matrices(A, B, C);

    // Write matrix C to file
    write_matrix("C.txt", C);

    // Free the allocated memory
    free_matrix(A);
    free_matrix(B);
    free_matrix(C);

    return 0;
}
