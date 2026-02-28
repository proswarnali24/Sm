#pragma once

#include <string>
#include <vector>

// Using the user's preferred macro style for loops
#define fr(i,n) for(int i = 0; i < n; i++)
#define fri(i,s,n) for(int i = s; i < n; i++)

// gets divisor code based on CRC type
inline std::string get_divisor(const std::string& crc_type){
    if (crc_type == "CRC-8")
        return "111010101"; // x^8 + x^7 + x^6 + x^4 + x^2 + 1
    else if (crc_type == "CRC-10")
        return "11000110011"; // x^10 + x^9 + x^5 + x^4 + x + 1
    else if (crc_type == "CRC-16")
        return "11000000000000101"; // x^16 + x^15 + x^2 + 1
    else if (crc_type == "CRC-32")
        return "100000100110000010001110110110111"; // x^32 + ... + 1
    return "";
}

// performs xor operations on two bitstrings of the same length
inline std::string xor_operation(const std::string &a, const std::string &b){
    std::string result = "";
    fr(i,b.size()){
        result += (a[i] == b[i]) ? '0' : '1';
    }
    return result;
}

// calculates the CRC remainder
inline std::string crc_remainder(const std::string &data, const std::string &divisor){
    if (divisor.empty()) return "";
    int n = data.size();
    int m = divisor.size();
    
    // Create a mutable copy and append m-1 zeros
    std::string newData = data;
    fr(i, m-1) {
        newData += '0';
    }

    for (int i = 0; i <= (int)newData.size() - m; ++i){
        if (newData[i] == '1'){
            // Perform XOR operation on the slice of newData
            for (int j = 0; j < m; ++j) {
                newData[i+j] = (newData[i+j] == divisor[j]) ? '0' : '1';
            }
        }
    }

    return newData.substr(newData.size() - (m - 1)); // Return the remainder
}

// generates CRC for each packet and appends it
inline std::vector<std::string> generate_crc(const std::vector<std::string> &packets, const std::string &crc_type){
    std::string divisor = get_divisor(crc_type);
    std::vector<std::string> result;
    if (divisor.empty()) return packets;

    for (const std::string &packet : packets){
        std::string remainder = crc_remainder(packet, divisor);
        result.push_back(packet + remainder);
    }

    return result;
}

// validate incoming packets using CRC
inline bool validate_crc(const std::string &packet_with_crc, const std::string &crc_type){
    std::string divisor = get_divisor(crc_type);
    if (divisor.empty()) return false;
    
    std::string remainder = crc_remainder(packet_with_crc, divisor);

    // If the remainder contains any '1', the check fails
    if (remainder.find('1') != std::string::npos)
        return false;

    return true;
}
