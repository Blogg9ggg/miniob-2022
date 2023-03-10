/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by wangyunlai on 2021/6/11.
//

#include <string.h>
#include <algorithm>

#include "common/log/log.h"

const double epsilon = 1E-6;

/*
 * 作者: 李立基
 * 说明: 简单的模糊匹配.
 * TODO: 如果字符串很长再加上记忆化.
 */
bool dfs(const char *target, int len1, int ind1, 
  const char *pattern, int len2, int ind2, int cntc, int cntt)
{
  if (ind2 == len2)
    return ind1 == len1;

  if (pattern[ind2] == '%') {
    if (ind2 + 1 == len2)
      return true;

    if (pattern[ind2 + 1] == '%') {
      return dfs(target, len1, ind1, 
      pattern, len2, ind2 + 1, cntc, cntt - 1);
    }
    if (cntt == 1) {
      if (ind1 + cntc > len1)
        return false;

      return dfs(target, len1, len1 - cntc, 
      pattern, len2, ind2 + 1, cntc, cntt - 1);
    }
   
    for (int i = 0; ind1 + i + cntc <= len1; i++) {
      if ((pattern[ind2 + 1] == '_' || pattern[ind2 + 1] == target[ind1 + i]) 
      && dfs(target, len1, ind1 + i, pattern, len2, ind2 + 1, cntc, cntt - 1)) {
        return true;
      }
    }
    return false;
  }

  while (ind1 < len1 && ind2 < len2) {
    if (pattern[ind2] == '%') 
      return dfs(target, len1, ind1, pattern, len2, ind2, cntc, cntt);

    if (pattern[ind2] == '_' || pattern[ind2] == target[ind1]) {
      ind1++;
      ind2++;
      cntc--;
    } else {
      return false;
    }
  }
  while (ind2 < len2 && pattern[ind2] == '%')
    ind2++;

  return ind1 == len1 && ind2 == len2;
}
int lcompare_string(void *arg1, void *arg2)
{
  const char *s1 = (const char *)arg1;
  const char *s2 = (const char *)arg2;
  int len1 = strlen(s1);
  int len2 = strlen(s2);

  LOG_INFO("s1(%d) = %s, s2(%d) = %s\n", len1, s1, len2, s2);
  int cntc = 0, cntt = 0;
  for (int i = 0; i < len2; i++) {
    if (s2[i] == '%')
      cntt++;
    else
      cntc++;
  }

  return dfs(s1, len1, 0, s2, len2, 0, cntc, cntt) ? 0 : 1;
}

int compare_int(void *arg1, void *arg2)
{
  int v1 = *(int *)arg1;
  int v2 = *(int *)arg2;
  return v1 - v2;
}

int compare_float(void *arg1, void *arg2)
{
  float v1 = *(float *)arg1;
  float v2 = *(float *)arg2;
  float cmp = v1 - v2;
  if (cmp > epsilon) {
    return 1;
  }
  if (cmp < -epsilon) {
    return -1;
  }
  return 0;
}

int compare_string(void *arg1, int arg1_max_length, void *arg2, int arg2_max_length)
{
  const char *s1 = (const char *)arg1;
  const char *s2 = (const char *)arg2;
  int maxlen = std::min(arg1_max_length, arg2_max_length);
  int result = strncmp(s1, s2, maxlen);
  if (0 != result) {
    return result;
  }

  if (arg1_max_length > maxlen) {
    return s1[maxlen] - 0;
  }

  if (arg2_max_length > maxlen) {
    return 0 - s2[maxlen];
  }
  return 0;
}
