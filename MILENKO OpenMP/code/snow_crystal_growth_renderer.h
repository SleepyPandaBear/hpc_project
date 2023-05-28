#ifndef SNOW_CRYSTAL_GROWTH_RENDERER_H
#define SNOW_CRYSTAL_GROWTH_RENDERER_H

#include "snow_crystal_growth_model.h"

fvec2
GridToPixel(grid *Grid, uvec2 CellIndex)
{
    fvec2 Result = {};

    Result.X = Grid->CellSize * (3.0f/2.0f) * CellIndex.Column;
    Result.Y = Grid->CellSize * sqrt(3.0f) * (CellIndex.Row - 0.5f * (CellIndex.Column&1));

    return Result;
}

void
SortTrianglePoints(ivec2 *P0, ivec2 *P1, ivec2 *P2)
{
    ivec2 Temp = {};

    if(P1->Y < P0->Y && P1->Y < P2->Y)
    {
        // P1 is MIN
        Temp = *P0;
        *P0 = *P1;
        *P1 = Temp;
    }
    else if(P2->Y < P0->Y && P2->Y < P1->Y)
    {
        // P@ is MIN
        Temp = *P0;
        *P0 = *P2;
        *P2 = Temp;
    }

    if(P1->Y > P2->Y)
    {
        Temp = *P1;
        *P1 = *P2;
        *P2 = Temp;
    }
}

void
DrawHorizontalLine(image *Image, color Color, ivec2 P0, ivec2 P1)
{
    // TODO(miha): Assert P0.Y == P1.Y
    if(P0.Y < 0 || (u32)P0.Y > Image->Height)
        return;

    for(i32 Start = P0.X; Start <= P1.X; ++Start)
    {
        if(Start < 0 || (u32)Start > Image->Width)
            continue;

        Image->Pixels[(Start*3) + P0.Y*(Image->Width*3) + 0] = Color.R;
        Image->Pixels[(Start*3) + P0.Y*(Image->Width*3) + 1] = Color.G;
        Image->Pixels[(Start*3) + P0.Y*(Image->Width*3) + 2] = Color.B;
    }
}

void
DrawTriangleTopFlat(image *Image, color Color, ivec2 P0, ivec2 P1, ivec2 P2)
{
    f32 InverseSlope0 = ((f32)P2.X - P0.X) / ((f32)P2.Y - P0.Y);
    f32 InverseSlope1 = ((f32)P2.X - P1.X) / ((f32)P2.Y - P1.Y);

    f32 CurrentX0 = P2.X;
    f32 CurrentX1 = P2.X;

    for(i32 ScanlineY = P2.Y; ScanlineY > P0.Y; --ScanlineY)
    {
        ivec2 P0 = {(i32)CurrentX0, ScanlineY};
        ivec2 P1 = {(i32)CurrentX1, ScanlineY};
        DrawHorizontalLine(Image, Color, P0, P1);
        CurrentX0 -= InverseSlope0;
        CurrentX1 -= InverseSlope1;
    }
}

void
DrawTriangleBottomFlat(image *Image, color Color, ivec2 P0, ivec2 P1, ivec2 P2)
{
    f32 InverseSlope0 = ((f32)P1.X - P0.X) / ((f32)P1.Y - P0.Y); // (v2.x - v1.x) / (v2.y - v1.y);
    f32 InverseSlope1 = ((f32)P2.X - P0.X) / ((f32)P2.Y - P0.Y); // (v3.x - v1.x) / (v3.y - v1.y);

    f32 CurrentX0 = P0.X;
    f32 CurrentX1 = P0.X;

    for(i32 ScanlineY = P0.Y; ScanlineY <= P1.Y; ++ScanlineY)
    {
        ivec2 P0 = {(i32)CurrentX0, ScanlineY};
        ivec2 P1 = {(i32)CurrentX1, ScanlineY};
        DrawHorizontalLine(Image, Color, P0, P1);
        CurrentX0 += InverseSlope0;
        CurrentX1 += InverseSlope1;
    }
}

ivec2 *
FindCellCornerPixels(grid *Grid, uvec2 CellIndex)
{
    ivec2 *Corners = (ivec2 *)malloc(6*sizeof(ivec2));
    fvec2 CellPixel = GridToPixel(Grid, CellIndex);

    for(u32 CornerIndex = 0; CornerIndex < 6; ++CornerIndex)
    {
        f32 Angle = 2.0f * M_PI * CornerIndex / 6;
        Corners[CornerIndex] = ivec2{(i32)(CellPixel.X + (Grid->CellSize * cos(Angle))), (i32)(CellPixel.Y + (Grid->CellSize * sin(Angle) * -1.0f))};
    }

    return Corners;
}

ivec2
ClampIVec2(ivec2 A, ivec2 Min, ivec2 Max)
{
    ivec2 Result = A;
    if(Result.X < Min.X)
       Result.X = Min.X;
    if(Result.X > Max.X)
        Result.X = Max.X;
    if(Result.Y < Min.Y)
       Result.Y = Min.Y;
    if(Result.Y > Max.Y)
        Result.Y = Max.Y;
    return Result;
}

void
DrawCell(grid *Grid, image *Image, uvec2 CellIndex)
{
    ivec2 *CornersPixels = FindCellCornerPixels(Grid, CellIndex);
    fvec2 CentrePixelF = GridToPixel(Grid, CellIndex);
    ivec2 CentrePixel = ivec2{(i32)CentrePixelF.X, (i32)CentrePixelF.Y};

    ivec2 C = ClampIVec2(CentrePixel, ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C0 = ClampIVec2(CornersPixels[0], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C1 = ClampIVec2(CornersPixels[1], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C2 = ClampIVec2(CornersPixels[2], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C3 = ClampIVec2(CornersPixels[3], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C4 = ClampIVec2(CornersPixels[4], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});
    ivec2 C5 = ClampIVec2(CornersPixels[5], ivec2{0,0}, ivec2{(i32)Image->Width, (i32)Image->Height});

    color Color{};
    cell Cell = GridElement(Grid, CellIndex.Row, CellIndex.Column);
    switch(Cell.Type)
    {
        case FROZEN:
        {
            Color = Green;
        } break;
        case BOUNDARY:
        {
            Color = Blue;
        } break;
        case EDGE:
        {
            Color = Orange;
        } break;
        case UNRECEPTIVE:
        {
            Color = White;
        } break;
    }

    ivec2 X = {};
    ivec2 Y = {};
    ivec2 Z = {};

    // T0
    X = C;
    Y = C0;
    Z = C1;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleBottomFlat(Image, Color, X, Z, Y);
    //printf("draw bot: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", X.X, X.Y, Y.X, Y.Y, Z.X, Z.Y);
    // T1
    X = C;
    Y = C1;
    Z = C2;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleTopFlat(Image, Color, Z, Y, X);
    //printf("draw bot: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", X.X, X.Y, Y.X, Y.Y, Z.X, Z.Y);
    // T2
    X = C;
    Y = C2;
    Z = C3;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleBottomFlat(Image, Color, X, Z, Y);
    //printf("draw bot: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", X.X, X.Y, Y.X, Y.Y, Z.X, Z.Y);
    // T3
    X = C;
    Y = C3;
    Z = C4;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleTopFlat(Image, Color, Y, X, Z);
    //printf("draw bot: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", X.X, X.Y, Y.X, Y.Y, Z.X, Z.Y);
    // T4
    X = C;
    Y = C4;
    Z = C5;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleBottomFlat(Image, Color, X, Y, Z);
    //printf("draw bot: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", X.X, X.Y, Y.X, Y.Y, Z.X, Z.Y);
    // T5
    X = C;
    Y = C0;
    Z = C5;
    SortTrianglePoints(&X,&Y,&Z);
    DrawTriangleTopFlat(Image, Color, X, Y, Z);
    //printf("draw top: p0: {%d, %d}, p1: {%d, %d}, p2: {%d, %d}\n", Y.X, Y.Y, Z.X, Z.Y, X.X, X.Y);
}

void
DrawGrid(grid *Grid, image *Image)
{
    for(u32 RowIndex = 0; RowIndex < Grid->Size; ++RowIndex)
    {
        for(u32 ColumnIndex = 0; ColumnIndex < Grid->Size; ++ColumnIndex)
        {
            DrawCell(Grid, Image, uvec2{RowIndex,ColumnIndex});
        }
    }
}

#endif // SNOW_CRYSTAL_GROWTH_RENDERER_H
