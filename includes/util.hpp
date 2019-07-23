//
// Created by ganler-Mac on 2019/7/23.
//
#pragma once

//#define GANLER_DEBUG

#ifdef GANLER_DEBUG
#include <iostream>
#define GANLER_DEBUG_DETAIL(X) { std::cout << "line: "<< __LINE__ << "\t " << (#X) << " is " << (X) << '\n'; }
#else
#define GANLER_DEBUG_DETAIL(X) {}
#endif