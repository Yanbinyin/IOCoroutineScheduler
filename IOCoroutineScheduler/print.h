/**
 * @file print.h
 * @author yinyb (990900296@qq.com)
 * @brief print
 * @version 1.0
 * @date 2022-04-03
 * @copyright Copyright (c) {2022}
 */

#include <iostream>
#include <string>

void print(std::string str = "", int num = 1) {
  for (int i = 0; i < num; ++i)
    std::cout << str;
}

void printL(std::string str = "", int num = 1) {
  for (int i = 0; i < num; ++i)
    std::cout << str;
  std::cout << std::endl;
}

// 打印分割线-----------
void printLine(std::string str = "", int num = 60) {
  print("-", num);
  printL(str);
}

void printPrefix(std::string prefix, int num = 60) {
  print(prefix);
  printL("-", num);
}

void printPostfix(std::string postfix, int num = 60) {
  print("-", num);
  printL(postfix);
}

// 打印等号 ===============
void printEql(int num = 60) { printL("=", num); }
