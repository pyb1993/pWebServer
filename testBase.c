//
//  testBase.c
//  pWenServer
//
//  Created by pyb on 2018/2/26.
//  Copyright © 2018年 pyb. All rights reserved.
//

#include "testBase.h"
bool int_eq(int expected, int actual){
    return expected == actual;
}

bool float_eq(float expected, float actual){
    return expected == actual;
}

void pushIntToVec(vector * v,int x){
    *(int*)vectorPush(v) = x;
}

int IntegerFromVec(vector* v,int pos){
    return *(int*)(vectorAt(v,pos));
}


/*******test chunk**********/
#define SET_CHUNK_UNIT(c,width,pos,x,type){\
    uint8_t* ele = c->data;\
    ele = ele + width * pos;\
    *(type *)ele = x;\
}

#define GET_CHUNK_UNIT(c,width,pos,type){\
    uint8_t* ele = c->data;\
    ele = ele + width * pos;\
    return *(type*)ele;\
}

void setChunkUnitInt(chunk*c,int width,int pos,int x){
    //SET_CHUNK_UNIT(c, width,pos, x, int);
    uint8_t* ele = c->data;
    ele = ele + width * pos;
     *(int*)ele = x;

}

int getChunkUnitInt(chunk*c,int width,int pos){
    GET_CHUNK_UNIT(c,width,pos,int);
}

void setChunkUnitFlt(chunk*c,int width,int pos,float x){
    SET_CHUNK_UNIT(c, width,pos, x, float);
}

float getChunkUnitFlt(chunk*c,int width,int pos){
    GET_CHUNK_UNIT(c,width,pos,float);
}


void test_flt(float expected,float actual){
    TEST_ANY_EQ(expected, actual,"%f", float_eq);
}

void test_int(int expected, int actual){
    TEST_ANY_EQ(expected, actual,"%d", int_eq);
}

