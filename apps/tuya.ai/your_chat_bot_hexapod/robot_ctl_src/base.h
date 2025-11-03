//
// 基础数据结构定义
// 用于六足机器人控制系统
//

#pragma once

#include <stdint.h>
#include <stdbool.h>

// 三维坐标点结构体
typedef struct {
    float x;  // X坐标
    float y;  // Y坐标  
    float z;  // Z坐标
} Point3D_t;

// 六足机器人腿部位置集合结构体
typedef struct {
    Point3D_t points[6];  // 六条腿的位置
} Locations_t;

// 运动表结构体
typedef struct {
    const Locations_t* table;      // 位置数据表
    int length;                    // 数据表长度
    int stepDuration;              // 单步持续时间(毫秒)
    const int* entries;            // 入口点数组
    int entriesCount;              // 入口点数量
} MovementTable_t;

// 函数声明

/**
 * @brief 创建三维坐标点
 * @param x X坐标
 * @param y Y坐标
 * @param z Z坐标
 * @return 创建的点
 */
Point3D_t point3d_create(float x, float y, float z);

/**
 * @brief 三维坐标点相加
 * @param a 第一个点
 * @param b 第二个点
 * @return 相加结果
 */
Point3D_t point3d_add(const Point3D_t* a, const Point3D_t* b);

/**
 * @brief 三维坐标点相减
 * @param a 被减数点
 * @param b 减数点
 * @return 相减结果
 */
Point3D_t point3d_subtract(const Point3D_t* a, const Point3D_t* b);

/**
 * @brief 三维坐标点赋值相加
 * @param target 目标点（会被修改）
 * @param other 要相加的点
 */
void point3d_add_assign(Point3D_t* target, const Point3D_t* other);

/**
 * @brief 三维坐标点赋值相减
 * @param target 目标点（会被修改）
 * @param other 要相减的点
 */
void point3d_subtract_assign(Point3D_t* target, const Point3D_t* other);

/**
 * @brief 计算两点间距离
 * @param a 第一个点
 * @param b 第二个点
 * @return 距离值
 */
float point3d_distance(const Point3D_t* a, const Point3D_t* b);

/**
 * @brief 创建六足位置集合
 * @param points 六个点的数组
 * @return 创建的位置集合
 */
Locations_t locations_create(const Point3D_t points[6]);

/**
 * @brief 创建运动表
 * @param table 位置数据表
 * @param length 数据表长度
 * @param stepDuration 单步持续时间
 * @param entries 入口点数组
 * @param entriesCount 入口点数量
 * @return 创建的运动表
 */
MovementTable_t movement_table_create(const Locations_t* table, int length, 
                                     int stepDuration, const int* entries, int entriesCount);
