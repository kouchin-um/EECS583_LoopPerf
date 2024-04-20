import math
import argparse
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
        error = (max(number1, number2) - min(number2 - number1)) / (max(number1, number2))
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

    if args.benchmark == 'sum_to_n':
        compare_sum(args.filename1, args.filename2)
    elif args.benchmark == 'image_blur':
        print("executing image_blur comparison")
    elif args.benchmark == 'test':
        print("executing test comparison")
    else:
        print("error benchmark name")

if __name__ == "__main__":
    main()

