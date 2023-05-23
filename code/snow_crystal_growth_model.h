#ifndef SNOW_CRYSTAL_GROWTH_MODEL_H
#define SNOW_CRYSTAL_GROWTH_MODEL_H

#include "stdint.h"

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t  i8;

typedef u32 b32;

typedef double f64;
typedef float f32;

union uvec3
{
    struct
    {
        u32 X;
        u32 Y;
        u32 Z;
    };
    struct
    {
        u32 R;
        u32 G;
        u32 B;
    };
    u32 E[3];
};

union ivec2
{
    struct
    {
        i32 X;
        i32 Y;
    };
    struct
    {
        u32 Row;
        u32 Column;
    };
    i32 E[2];
};

inline ivec2
operator*(f32 A, ivec2 B)
{
    ivec2 Result = {};
    Result.X = A*B.X;
    Result.Y = A*B.Y;
    return Result;
}

inline ivec2
operator*(ivec2 B, f32 A)
{
    ivec2 Result = {};
    Result = A*B;
    return Result;
}

union uvec2
{
    struct
    {
        u32 X;
        u32 Y;
    };
    struct
    {
        u32 Row;
        u32 Column;
    };
    u32 E[2];
};

union fvec2
{
    struct
    {
        f32 X;
        f32 Y;
    };
    f32 E[2];
};

enum direction
{
    SE,
    NE,
    N,
    NW,
    SW,
    S
};

enum cell_type
{
    UNRECEPTIVE,
    FROZEN,
    BOUNDARY,
    EDGE,
};

union color
{
    struct
    {
        u8 R;
        u8 G;
        u8 B;
    };
    u8 E[3];
};

static color Green = {0, 255, 0};
static color Blue = {0, 0, 255};
static color Orange = {255, 0, 0};
static color White = {255, 255, 255};

struct image
{
    char Filename[256];
    u8 *Pixels;
    u32 ChannelsPerPixel;
    u32 Width;
    u32 Height;
    u32 ToPixelMultiplier;
};

struct cell
{
    cell_type Type;
    f32 Value;
};

struct grid
{
    cell *Cells;
    u32 Size;
    u32 CellSize;
};

cell
GridElement(grid *Grid, u32 Row, u32 Column)
{
    return Grid->Cells[Row*Grid->Size + Column];
}

void
GridSetElement(grid *Grid, u32 Row, u32 Column, cell Cell)
{
    if(Column & 1)
    {
        // NOTE(miha): Even column.
        Grid->Cells[(Row+1)*Grid->Size + Column] = Cell;
    }
    else
    {
        // NOTE(miha): Odd column.
    }
}

ivec2
GridNeighbour(u32 Row, u32 Column, u32 Direction)
{
    i32 DirectionDifference[2][6][2] = {
        // Even column
        {{1, 1}, {1, 0}, {0, -1}, {-1, 0}, {-1, 1}, {0, 1}},
        // Odd columns
        {{1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {0, 1}}};
    u32 Parity = Column & 1;
    i32 *Difference = DirectionDifference[Parity][Direction];
    return ivec2{(i32)Row+Difference[1], (i32)Column+Difference[0]};
}

#endif // SNOW_CRYSTAL_GROWTH_MODEL_H
