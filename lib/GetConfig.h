//
//  GetConfig.h
//  paxosmem
//
//  Created by didi on 2018/8/1.
//  Copyright © 2018年 didi. All rights reserved.
//

#ifndef GetConfig_h
#define GetConfig_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 数据类型重定义
typedef signed   int    INT32;
typedef unsigned int    UINT32;

// 函数声明
void GetCompletePath(char *pszConfigFileName, char *pszWholePath);
void GetStringContentValue(FILE *fp, char *pszSectionName,char *pszKeyName, char *pszOutput, UINT32 iOutputLen);
void GetConfigFileStringValue(char *pszSectionName, char *pszKeyName, char *pDefaultVal, char *pszOutput, UINT32 iOutputLen, char *pszConfigFileName);
INT32 GetConfigFileIntValue(char *pszSectionName, char *pszKeyName, UINT32 iDefaultVal, char *pszConfigFileName);

#endif /* GetConfig_h */
