#pragma once

#include <iostream>
#include <string>
#include <cstdlib> // For rand()

// Protected region size: first 12 bytes = 96 bits (sender 48 + receiver 48)
static const int ADDRESS_REGION_BITS = 96; // do not touch these bits

inline void singlebit_random_error(std::string &data)
{
    int n = data.length();
    if (n <= ADDRESS_REGION_BITS) return; // nothing to flip
    int random_index = ADDRESS_REGION_BITS + (rand() % (n - ADDRESS_REGION_BITS));
    if (data[random_index] == '0') data[random_index] = '1';
    else data[random_index] = '0';
}

inline void singlebit_error(std::string &data, int index)
{
    if (index < ADDRESS_REGION_BITS) {
        std::cout << "Index inside address region. Adjusting to first payload bit.\n";
        index = ADDRESS_REGION_BITS;
    }
    if (index >= (int)data.length()) {
        std::cout << "Index out of range. No change.\n";
        return;
    }
    if (data[index] == '0') data[index] = '1';
    else data[index] = '0';
}

inline void isolated_doublebit_error(std::string &data, int index1, int index2)
{
    if (index1 < ADDRESS_REGION_BITS) { std::cout << "Index1 inside address region. Adjusting.\n"; index1 = ADDRESS_REGION_BITS; }
    if (index2 < ADDRESS_REGION_BITS) { std::cout << "Index2 inside address region. Adjusting.\n"; index2 = ADDRESS_REGION_BITS; }
    if (index1 >= (int)data.length() || index2 >= (int)data.length()) {
        std::cout << "Index out of range. No change.\n";
        return;
    }
    if (data[index1] == '0') data[index1] = '1';
    else data[index1] = '0';
    if (data[index2] == '0') data[index2] = '1';
    else data[index2] = '0';
}

inline void odd_errors(std::string &data , int n)
{
    if (n % 2 == 0 && n > 0) {
        std::cout << "Number of errors must be odd. Adjusting to " << n - 1 << " errors.\n";
        n--;
    }
    while(n-- > 0)
    {
        if(data.length() <= ADDRESS_REGION_BITS) continue;
        int random_index = ADDRESS_REGION_BITS + (rand() % (data.length() - ADDRESS_REGION_BITS));
        if (data[random_index] == '0') data[random_index] = '1';
        else data[random_index] = '0';
    }
}

inline void burst_error(std::string &data, int start, int end)
{
    if (start < ADDRESS_REGION_BITS) {
        std::cout << "Burst start inside address region. Adjusting start to first payload bit.\n";
        start = ADDRESS_REGION_BITS;
    }
    if (end < ADDRESS_REGION_BITS) {
        std::cout << "Burst end inside address region. Adjusting.\n";
        end = ADDRESS_REGION_BITS;
    }
    if (start >= (int)data.length() || end >= (int)data.length() || start > end) {
        std::cout << "Invalid burst indices. No change.\n";
        return;
    }
    for (int i = start; i <= end; i++)
    {
        if (data[i] == '0') data[i] = '1';
        else data[i] = '0';
    }
}

inline void ask(std::string &data)
{
    while(true){
        std::cout << "Enter the type of error you want to inject: " << std::endl;
        std::cout << "1. Single bit random error (addresses protected)" << std::endl;
        std::cout << "2. Single bit error" << std::endl;
        std::cout << "3. Isolated double bit error" << std::endl;
        std::cout << "4. Odd number of errors" << std::endl;
        std::cout << "5. Burst error" << std::endl;
        std::cout << "6. No Error" << std::endl;
        std::cout << "7. Exit error injection menu" << std::endl;
        std::cout << "Enter your choice: ";
        int choice;
        std::cin >> choice;
        switch(choice)
        {
            case 1:
                singlebit_random_error(data);
                break;
            case 2:
                std::cout << "Enter the index at which you want to inject error (bit index, 0-based): ";
                {
                    int index;
                    std::cin >> index;
                    singlebit_error(data, index);
                }
                break;
            case 3:
                std::cout << "Enter the indices at which you want to inject error (bit indices, 0-based): ";
                {
                    int index1, index2;
                    std::cin >> index1 >> index2;
                    isolated_doublebit_error(data, index1, index2);
                }
                break;
            case 4:
                std::cout << "Enter the number of errors you want to inject: ";
                {
                    int n;
                    std::cin >> n;
                    odd_errors(data, n);
                }
                break;
            case 5:
                std::cout << "Enter the start and end index of the burst error (bit indices, 0-based): ";
                {
                    int start, end;
                    std::cin >> start >> end;
                    burst_error(data, start, end);
                }
                break;
            case 6:
                // No error, just continue
                break;
            case 7:
                return;
            default:
                std::cout << "Invalid choice\n";
        }
    }
}
