//  
//  main.c  
//  CAESTest  
//  
//  Created by comeontom on 14/7/31.  
//  Copyright (c) 2014年 comeontom. All rights reserved.  
//  
  
#include <stdio.h>  
#include <unistd.h>  
#include <string.h>  
#include <stdlib.h>  
#include <CommonCrypto/CommonCryptor.h>  
  
#import <CommonCrypto/CommonDigest.h>  
//#import <CommonCrypto/CommonHMAC.h>  
  
char cryptKey[kCCKeySizeAES128] = "O3fX{j{{*1:r[S2w";  
  
char hexChar(unsigned char c) {  
    return c < 10 ? '0' + c : 'a' + c - 10;  
}  
  
void hexString32(unsigned char *from, char *to, int length) {  
    for (int i = 0; i < length; ++i) {  
        unsigned char c = from[i];  
        unsigned char cHigh = c >> 4;  
        unsigned char cLow = c & 0xf;  
        to[2 * i] = hexChar(cHigh);  
        to[2 * i + 1] = hexChar(cLow);  
    }  
    to[2 * length] = '\0';  
}  
  
  
  
int main(int argc, const char * argv[])  
{  
    const char* original_str = "comeontom";  
    unsigned char result[CC_MD5_DIGEST_LENGTH];  
    CC_MD5(original_str, (CC_LONG)strlen(original_str), result);  
      
    char hexResult32[2 * CC_MD5_DIGEST_LENGTH + 1];  
    hexString32(result, hexResult32, CC_MD5_DIGEST_LENGTH);  
    printf("%s\n",hexResult32);  
    return 0;//返回0;正常退出  
}  
