#include "base.h"
#include <math.h>

// 创建三维坐标点
Point3D_t point3d_create(float x, float y, float z) {
    Point3D_t point;
    point.x = x;
    point.y = y;
    point.z = z;
    return point;
}

// 三维坐标点相加
Point3D_t point3d_add(const Point3D_t* a, const Point3D_t* b) {
    return point3d_create(a->x + b->x, a->y + b->y, a->z + b->z);
}

// 三维坐标点相减
Point3D_t point3d_subtract(const Point3D_t* a, const Point3D_t* b) {
    return point3d_create(a->x - b->x, a->y - b->y, a->z - b->z);
}

// 三维坐标点赋值相加
void point3d_add_assign(Point3D_t* target, const Point3D_t* other) {
    target->x += other->x;
    target->y += other->y;
    target->z += other->z;
}

// 三维坐标点赋值相减
void point3d_subtract_assign(Point3D_t* target, const Point3D_t* other) {
    target->x -= other->x;
    target->y -= other->y;
    target->z -= other->z;
}

// 计算两点间距离
float point3d_distance(const Point3D_t* a, const Point3D_t* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

// 创建六足位置集合
Locations_t locations_create(const Point3D_t points[6]) {
    Locations_t locations;
    for (int i = 0; i < 6; i++) {
        locations.points[i] = points[i];
    }
    return locations;
}

// 创建运动表
MovementTable_t movement_table_create(const Locations_t* table, int length, 
                                     int stepDuration, const int* entries, int entriesCount) {
    MovementTable_t movement_table;
    movement_table.table = table;
    movement_table.length = length;
    movement_table.stepDuration = stepDuration;
    movement_table.entries = entries;
    movement_table.entriesCount = entriesCount;
    return movement_table;
}
