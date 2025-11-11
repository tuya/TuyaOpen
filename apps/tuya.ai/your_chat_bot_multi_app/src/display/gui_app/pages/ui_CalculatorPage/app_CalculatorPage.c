#include "app_CalculatorPage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


uint8_t strput(StrStack_t * st,char strin)
{
    // 修复：strque只有10个元素（索引0-9），应该检查 >= 10
    if(st->Top_Point >= 10)
    {return -1;}

    st->strque[st->Top_Point++] = strin;
    return 0;
}

uint8_t strdel(StrStack_t * st)
{
    if(st->Top_Point == 0)
    {return -1;}

    st->strque[--st->Top_Point] = 0;
    return 0;
}

uint8_t strstack_isEmpty(StrStack_t* st)
{
    if(st->Top_Point == 0)
    {return 1;}

    return 0;
}

void strclear(StrStack_t* sq)
{
    while(!strstack_isEmpty(sq))
    {
        strdel(sq);
    }
}

uint8_t NumStackPut(NumStack_t * st, float in)
{
   if(st->Top_Point == CAL_DEPTH - 1)
	{return -1;}

	st->data[st->Top_Point++] = in;
	return 0;
}

uint8_t NumStackDel(NumStack_t * st)
{
    if(st->Top_Point == 0)
	{return -1;}
    // 修复：应该先递减再访问，避免越界
    st->Top_Point--;
    st->data[st->Top_Point] = 0;
	return 0;
}

uint8_t NumStack_isEmpty(NumStack_t* st)
{
	if(st->Top_Point == 0)
	{return 1;}

	return 0;
}

void NumStackClear(NumStack_t* st)
{
	while(!NumStack_isEmpty(st))
	{
		NumStackDel(st);
	}
}

uint8_t SymStackPut(SymStack_t * st, char in)
{
	if(st->Top_Point == CAL_DEPTH - 1)
	{return -1;}

	st->data[st->Top_Point++] = in;
	return 0;
}

uint8_t SymStackDel(SymStack_t * st)
{
  if(st->Top_Point == 0)
	{return -1;}
    // 修复：应该先递减再访问，避免越界
    st->Top_Point--;
    st->data[st->Top_Point] = 0;
	return 0;
}

uint8_t SymStack_isEmpty(SymStack_t* st)
{
	if(st->Top_Point == 0)
	{return 1;}

	return 0;
}

void SymStackClear(SymStack_t* st)
{
	while(!SymStack_isEmpty(st))
	{
		SymStackDel(st);
	}
}

uint8_t SymisHighPriority(char top, char present)
{
    //乘除的优先级最大
    if(top == '*' || top == '/')
    {
        return 1;
    }
    else if(top == '+')
    {
        if(present == '-')
        {return 1;}
        else
        {return 0;}
    }
    else if(top == '-')
    {
        if(present == '+')
        {return 1;}
        else
        {return 0;}
    }
    return 0;
}

void CalculateOne(NumStack_t * numstack, SymStack_t * symstack)
{
    // 修复：添加边界检查，确保栈有足够的元素
    if(symstack->Top_Point == 0 || numstack->Top_Point < 2)
    {
        return; // 栈元素不足，无法计算
    }
    
    caldata_t temp = {0};
    temp.datatype = NUMBER_TYPE;
    temp.symbol = 0;
    //计算数字栈中的顶部两数,结果存到temp中
    if(symstack->data[symstack->Top_Point-1] == '+')
        temp.number = (numstack->data[numstack->Top_Point-2]) + (numstack->data[numstack->Top_Point-1]);

    else if(symstack->data[symstack->Top_Point-1] == '-')
        temp.number = (numstack->data[numstack->Top_Point-2]) - (numstack->data[numstack->Top_Point-1]);

    else if(symstack->data[symstack->Top_Point-1] == '*')
        temp.number = (numstack->data[numstack->Top_Point-2]) * (numstack->data[numstack->Top_Point-1]);

    else if(symstack->data[symstack->Top_Point-1] == '/')
    {
        // 修复：检查除零错误
        if(numstack->data[numstack->Top_Point-1] == 0.0f)
        {
            return; // 除零错误，无法计算
        }
        temp.number = (numstack->data[numstack->Top_Point-2]) / (numstack->data[numstack->Top_Point-1]);
    }
    else
    {
        return; // 未知运算符
    }

    //运算前两数出栈,运算结果数入栈
    NumStackDel(numstack);
    NumStackDel(numstack);
    NumStackPut(numstack,temp.number);
    SymStackDel(symstack);

}

uint8_t NumSymSeparate(char * str, uint8_t strlen, NumStack_t * NumStack, SymStack_t * SymStack)
{
    // 修复：添加空指针检查
    if(str == NULL || NumStack == NULL || SymStack == NULL)
    {
        return 1; // 错误：空指针
    }
    
    // 修复：检查字符串长度，避免越界
    if(strlen == 0 || strlen > 10)
    {
        return 1; // 错误：字符串长度无效
    }
    
    // 修复：检查字符串第一个字符是否有效
    if(str[0] == '\0' || (str[0] > '9' || str[0] < '0'))
    {
        return 1; // 错误：第一个字符不是数字
    }
    
		NumStackClear(NumStack);
		SymStackClear(SymStack);
    caldata_t temp,temp_pre;
    char NumBehindPoint_Flag = 0;//数字是否在小数点后,后多少位
    temp.datatype = NUMBER_TYPE;
    temp.number = 0;
    temp.symbol = 0;
    temp_pre = temp;
    temp_pre.datatype = SYMBOL_TYPE;
    
		int i;
    // 修复：确保不越界，同时检查字符串是否以\0结尾
    for(i=0; i < strlen && i < 10 && str[i] != '\0'; i++)
    {
        if(str[i]=='.')
        {
            temp.datatype = POINT_TYPE;
            if(temp_pre.datatype == NUMBER_TYPE)
            {}
            else
            {return 2;}
            temp_pre = temp;
        }
        if(str[i]<='9' && str[i]>='0')
        {
            //溢出报错
            if(NumStack->Top_Point>CAL_DEPTH || SymStack->Top_Point>CAL_DEPTH)
            {return 3;}
            //读取当前的字符到temp中
            temp.datatype = NUMBER_TYPE;
            temp.number = (str[i] - '0');
            temp.symbol = 0;
            //如果为连续数字,需要进行进位,将数字栈顶读出进位，再加上现在位，再入栈
            if(temp_pre.datatype == NUMBER_TYPE)
            {
                // 修复：确保栈不为空
                if(NumStack->Top_Point == 0)
                {
                    return 6; // 错误：栈为空
                }
                if(!NumBehindPoint_Flag)
                {temp.number += NumStack->data[NumStack->Top_Point-1] * 10;}
                else
                {
                    NumBehindPoint_Flag += 1;
                    char i = NumBehindPoint_Flag;
                    while(i--)
                    {temp.number /= 10;}
                    temp.number += NumStack->data[NumStack->Top_Point-1];
                }
                NumStackDel(NumStack);
                NumStackPut(NumStack,temp.number);
            }
            //当前数字刚好是小数点后一位
            else if(temp_pre.datatype == POINT_TYPE)
            {
                // 修复：确保栈不为空
                if(NumStack->Top_Point == 0)
                {
                    return 7; // 错误：栈为空
                }
                NumBehindPoint_Flag = 1;
                temp.number /= 10;
                temp.number += NumStack->data[NumStack->Top_Point-1];
                NumStackDel(NumStack);
                NumStackPut(NumStack,temp.number);
            }
            //前一位不是数字或小数点,现在读取的这一位是数字，直接入栈
            else
            {
                NumStackPut(NumStack,temp.number);
            }
            temp_pre = temp;
        }
        else if(str[i] == '+' || str[i] == '-' || str[i] == '*' || str[i] == '/')
        {
            //溢出报错
            if(NumStack->Top_Point>CAL_DEPTH || SymStack->Top_Point>CAL_DEPTH)
            {return 4;}
            //读取当前的字符到temp中
            temp.datatype = SYMBOL_TYPE;
            temp.symbol = str[i];
            temp.number = 0;
            NumBehindPoint_Flag = 0;//小数点计算已经结束
            //重复输入了运算符号
            if(temp_pre.datatype == SYMBOL_TYPE)
            {
                return 5 ;//erro
            }
            else
            {
                if((!SymStack_isEmpty(SymStack)) && SymisHighPriority(SymStack->data[SymStack->Top_Point-1],temp.symbol))
                {
                    CalculateOne(NumStack, SymStack);
                    SymStackPut(SymStack,temp.symbol);
                }
                else
                {
                    //符号压入符号栈
                    SymStackPut(SymStack,temp.symbol);
                }
                temp_pre = temp;
            }
        }
    }
    return 0;
}

uint8_t StrCalculate(char * str,NumStack_t * NumStack, SymStack_t * SymStack)
{
    // 修复：检查字符串是否为空
    if(str == NULL || NumStack == NULL || SymStack == NULL)
    {
        return -1;
    }
    
    // 修复：手动计算字符串长度，避免使用strlen（可能在某些平台上不稳定）
    uint8_t str_len = 0;
    while(str_len < 10 && str[str_len] != '\0')
    {
        str_len++;
    }
    
    if(str_len == 0)
    {
        return -1;
    }
    
    if(NumSymSeparate(str, str_len, NumStack, SymStack))
    {
        //erro, clear all
        NumStackClear(NumStack);
        SymStackClear(SymStack);
        return -1;
    }
    else
    {
        // 修复：添加安全检查，防止无限循环
        int max_iterations = 100; // 最大迭代次数，防止死循环
        while(!SymStack_isEmpty(SymStack) && max_iterations-- > 0)
        {
            // 修复：检查是否有足够的数字进行计算
            if(NumStack->Top_Point < 2)
            {
                // 数字不足，无法继续计算
                NumStackClear(NumStack);
                SymStackClear(SymStack);
                return -1;
            }
            CalculateOne(NumStack,SymStack);
        }
        
        // 如果循环退出但栈不为空，说明有错误
        if(!SymStack_isEmpty(SymStack))
        {
            NumStackClear(NumStack);
            SymStackClear(SymStack);
            return -1;
        }
    }
    return 0;
}

uint8_t isIntNumber(float number)
{
    // 修复：使用更安全的浮点数比较方法
    float diff = number - (float)(int)number;
    if(diff < 0.0f) diff = -diff;
    if(diff < 0.0001f)  // 允许小的浮点误差
    {
        return 1;
    }
	return 0;
}


