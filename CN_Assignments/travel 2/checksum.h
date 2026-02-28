#pragma once

#include <string>
#include <vector>

// Using the user's preferred macro style for loops
#define fr(i,n) for(int i = 0; i < n; i++)
#define fri(i,s,n) for(int i = s; i < n; i++)

// Helper for one's complement. Assumes data is a bitstring.
inline std::string onesComplement(std::string data){
    fr(i,data.size()){
        if(data[i] == '0'){
            data[i] = '1';
        }
        else{
            data[i] = '0';
        }
    }
    return data;
}

// 1) Generate checksum
// Assumes data is a bitstring (e.g., "01101001...")
inline std::string generate_checksum(std::string data, int blockSize){
    if (blockSize <= 0) return "";

    // Pad data with '0's if its length is not a multiple of blockSize
    if (data.size() % blockSize != 0) {
        int padding = blockSize - (data.size() % blockSize);
        fr(i, padding) {
            data += '0';
        }
    }

    int n = data.size();
    if (n == 0) return std::string(blockSize, '1'); // Checksum of nothing is all ones

    std::string finalchecksum = data.substr(0, blockSize);
    
    int i = blockSize;
    while(i < n){
        std::string nextBlock = data.substr(i, blockSize);

        std::string intermediatechecksum = "";
        int carry = 0;
        for(int k = blockSize-1; k >= 0; k--){
            int sum = (nextBlock[k] - '0') + (finalchecksum[k] - '0') + carry;
            intermediatechecksum = char('0' + (sum%2)) + intermediatechecksum;
            carry = sum/2;
        }

        // Add wrap-around carry
        if(carry == 1){
            std::string final_sum = "";
            for(int k = intermediatechecksum.size()-1; k >= 0; k--){
                if(carry == 0){
                    final_sum = intermediatechecksum[k] + final_sum;
                }
                else if(((intermediatechecksum[k] - '0') + carry)%2 == 0){
                    final_sum = '0' + final_sum;
                    carry = 1;
                }
                else{
                    final_sum = '1' + final_sum;
                    carry = 0;
                }
            }
            finalchecksum = final_sum;
        } else {
            finalchecksum = intermediatechecksum;
        }

        i += blockSize;
    }

    return onesComplement(finalchecksum);
}

// 2) Validate checksum
// Assumes packets contain bitstrings
inline bool validate_checksum(const std::vector<std::string>& packets, const std::string& checksum){
    std::string data = "";
    for(const auto& a: packets){
        data += a;
    }

    int blockSize = checksum.size();
    if (blockSize == 0) return data.empty();

    // On the receiver side, the checksum of (data + received_checksum) should be all zeros.
    std::string receiverCheckSum = generate_checksum(data + checksum, blockSize);

    for(char c : receiverCheckSum){
        if(c != '0')
            return false; // If any bit is not '0', validation fails
    }

    return true;
}
