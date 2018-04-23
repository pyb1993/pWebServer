//
//  testBase.h
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#ifndef testBase_h
#define testBase_h

#include <stdio.h>
#include "globals.h"
#include "vector.h"
#include "chunk.h"

#define TEST_ANY_EQ(expected,actual,format,EQ)\
{if(!EQ(expected,actual)){\
    printf("TEST FAILED: expected = " format ",actual = " format "\n",expected,actual);}}

void pushIntToVec(vector * v,int x);
int IntegerFromVec(vector* v,int pos);
void setChunkUnitInt(chunk * c,int width,int pos, int x);
int getChunkUnitInt(chunk* c,int width,int pos);
void setChunkUnitFlt(chunk * c,int width,int pos, float x);
float getChunkUnitFlt(chunk* c,int width,int pos);

bool test_int_eq(int expected, int actual);
void test_int(int expected, int actual);
void test_flt(float expected,float actual);
#endif /* testBase_h */
