import math
import argparse

def read_matrix_from_file(filename):
    matrix = []
    with open(filename, 'r') as file:
        for line in file:
            # Convert each line to a list of floats
            row = list(map(float, line.split()))
            matrix.append(row)
    return matrix


# calculate ARE 
def calculate_are(matrix1, matrix2):
    total_error = 0
    count = 0
    for i in range(len(matrix1)):
        for j in range(len(matrix1[0])):
            if matrix1[i][j] != 0:  
                rel_error = abs((matrix1[i][j] - matrix2[i][j]) / matrix1[i][j])
                total_error += rel_error
                count += 1
            elif matrix2[i][j] != 0:
                return float('inf')  # If matrix1[i][j] is zero and matrix2[i][j] is not, error is infinite
    return total_error / count if count > 0 else 0


def compare_matrices(file1, file2):
    matrix1 = read_matrix_from_file(file1)
    matrix2 = read_matrix_from_file(file2)

    if matrix1 == matrix2:
        print("The matrices are identical.")
    else:
        print("The matrices are not identical.")
        are_error = calculate_are(matrix1, matrix2)
        print("ARE is : ", are_error)



def compare_sum(filename1, filename2):
    try:
        with open(filename1, 'r') as file1:
            content1 = file1.read().strip()  # Read the file content and strip whitespace
            number1 = int(content1) 
        with open(filename2, 'r') as file2:
            content2 = file2.read().strip()  
            number2 = int(content2)   
    except FileNotFoundError:
        print("Error: The file does not exist.")
        return
    except ValueError:
        print("Error: The file does not contain a valid integer.")
        return

    print("number 1: ", number1)
    print("number 2: ", number2)
    if number1 != number2:
        # some metrics to compare
        error = (max(number1, number2) - min(number2, number1)) / (max(number1, number2))
        print("error rate: ", error)

        # log scale:
        print("number 1 log scale: ", math.log(number1))
        print("number 2 log scale: ", math.log(number2))



def main():
    parser = argparse.ArgumentParser(description="Commpare files based on the benchmark command.")
    parser.add_argument('benchmark', type=str, help='Benchmark to compare')
    parser.add_argument('filename1', type=str, help='First filename')
    parser.add_argument('filename2', type=str, help='Second filename')

    args = parser.parse_args()

    if args.benchmark == 'sum-to-n':
        print("executing sum to n comparison")
        compare_sum(args.filename1, args.filename2)
    elif args.benchmark == 'mat-mul':
        print("executing mat-mul comparison")
        compare_matrices(args.filename1, args.filename2)
    elif args.benchmark == 'test':
        print("executing test comparison")
    else:
        print("error benchmark name")

if __name__ == "__main__":
    main()

