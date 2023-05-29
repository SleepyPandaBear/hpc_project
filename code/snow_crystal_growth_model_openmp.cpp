#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "snow_crystal_growth_model.h"

// NOTE(miha): Import OpenMP for timer.
#include "omp.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
//#define DRAW_CELL_BORDER
#include "snow_crystal_growth_renderer.h"

// #define SAVE_DURING_ITERATIONS
#define SAVE_DURING_ITERATIONS_INTERVAL (50)

// NOTE(miha): For grid we will use offset coordinates (even-q).

b32
GenerateGrid(grid *Grid, f32 Beta)
{
    // NOTE(miha): Grid has to be odd sized.
    if(Grid->Size % 2 == 0)
    {
        printf("Grid has to be odd sized, %d mod 2 != 1\n", Grid->Size);
        return 0;
    }

    cell *Cells = (cell *)malloc(Grid->Size*Grid->Size*sizeof(cell));
    memset(Cells, 0, Grid->Size*Grid->Size*sizeof(cell));

    // NOTE(miha): Set top row cells to EDGE
    for(u32 RowIndex = 0; RowIndex < Grid->Size; ++RowIndex)
    {
        Cells[RowIndex].Type = EDGE;
    }

    // NOTE(miha): Set bottom row cells to EDGE
    for(u32 RowIndex = 0; RowIndex < Grid->Size; ++RowIndex)
    {
        //printf("bottom: row_idx: %d\n", RowIndex);
        Cells[(Grid->Size-1)*Grid->Size + RowIndex].Type = EDGE;
    }

    // NOTE(miha): Set left column cells to EDGE
    for(u32 ColumnIndex = 0; ColumnIndex < Grid->Size; ++ColumnIndex)
    {
        Cells[ColumnIndex*Grid->Size].Type = EDGE;
    }

    // NOTE(miha): Set right column cells to EDGE
    for(u32 ColumnIndex = 0; ColumnIndex < Grid->Size; ++ColumnIndex)
    {
        //printf("right: col_idx: %d\n", ColumnIndex);
        Cells[(Grid->Size-1) + ColumnIndex*Grid->Size].Type = EDGE;
    }

    // NOTE(miha): Set all cell values to Beta.
    for(u32 CellIndex = 0; CellIndex < Grid->Size*Grid->Size; ++CellIndex)
    {
        Cells[CellIndex].Value = Beta;
    }

    // NOTE(miha): Set middle cell to FROZEN
    Cells[(Grid->Size/2)*Grid->Size + (Grid->Size/2)].Type = FROZEN;
    Cells[(Grid->Size/2)*Grid->Size + (Grid->Size/2)].Value = 1.0f;

    // TODO(miha): Think about this, is it better to set BOUANDY around seed at
    // the start or should it be calculated?
#if 0
    // NOTE(miha): Set neihbours of the middle cell to BOUNDARY
    for(u32 Direction = 0; Direction < 6; ++Direction)
    {
        ivec2 Neighbour = GridNeighbour(Grid->Size/2, Grid->Size/2, Direction);
        Cells[Neighbour.Row*Grid->Size + Neighbour.Column].Type = BOUNDARY;
    }
#endif

    Grid->Cells = Cells;

    return 1;
}

#if 0
void
PrintGrid(grid *Grid)
{
    for(u32 Row = 0; Row < Grid->Size; ++Row)
    {
        for(u32 Column = 0; Column < Grid->Size; ++Column)
        {
            if(Column % 2 == 0)
            {
                if(Row == 0)
                {
                    printf("%6.2f ", -1.0f);
                    //printf("%6.2f ", GridElement(Grid, Row, Column).Value);
                }
                else
                {
                    ivec2 Neighbour = GridNeighbour(Row, Column, N);
                    printf("%6.2f ", GridElement(Grid, Neighbour.X, Neighbour.Y).Value);
                    //printf("%6.2f ", GridElement(Grid, Row, Column).Value);
                }
            }
            else
            {
                printf("%6.2f ", GridElement(Grid, Row, Column).Value);
            }
        }
        printf("\n");
    }

    for(u32 Column = 0; Column < Grid->Size; ++Column)
    {
        if(Column % 2 == 0)
        {
            printf("%6.2f ", GridElement(Grid, Grid->Size-1, Column).Value);
        }
        else
        {
            printf("%6.2f ", -1.0f);
        }
    }
    printf("\n");
}
#endif

#if 0
void
FillGridWithTestElements(grid *Grid)
{
    for(u32 Row = 0; Row < Grid->Size; ++Row)
    {
        for(u32 Column = 0; Column < Grid->Size; ++Column)
        {
            Grid->Cells[Row*Grid->Size + Column].Value = (f32)(Row*Grid->Size + Column);
        }
    }
}
#endif

b32
IsReceptive(cell Cell)
{
    if(Cell.Type == FROZEN || Cell.Type == BOUNDARY)
        return 1;
    return 0;
}

// ivec2 GridNeighbour(row, col, dir)
i32
main(i32 ArgumentCount, char *ArgumentValues[])
{
    f32 Alpha = atof(ArgumentValues[1]);
    f32 Beta = atof(ArgumentValues[2]);
    f32 Gamma = atof(ArgumentValues[3]);
    u32 Size = atoi(ArgumentValues[4]);
    i32 MaxIteration = atoi(ArgumentValues[5]);

    grid Grid = {};
    // CARE(miha): CellSize is the radius of a cell!
    Grid.CellSize = 10;
    // CARE(miha): Border is included into Grid.Size!
    Grid.Size = Size;
    // NOTE(miha): We have two grids; one for time t and one for time t+1.
    grid NewGrid = {};
    NewGrid.CellSize = Grid.CellSize;
    NewGrid.Size = Grid.Size;
    if(GenerateGrid(&Grid, Beta) && GenerateGrid(&NewGrid, Beta))
    {
        image Image = {};
        Image.ToPixelMultiplier = Grid.CellSize;
        Image.ChannelsPerPixel = 3;
        Image.Width = Grid.Size * 1.5f * Image.ToPixelMultiplier;
        Image.Height = Grid.Size * sqrtf(3) * Image.ToPixelMultiplier;
        u8 *ImagePixels = (u8 *)malloc(Image.Width*Image.Height*Image.ChannelsPerPixel);
        Image.Pixels = ImagePixels;

        b32 Running = 1;
        u32 Iteration = 0;
        u32 FromIterations = 0;

        // NOTE(miha): First iteration we do calculations in the 'NextGrid'.
        grid *CurrentGrid = &Grid;
        grid *NextGrid = &NewGrid;

        f64 TimeStart = omp_get_wtime();
        while(Running)
        {
            u32 MaxColumn = 0;
            // printf("iteration: %d\n", Iteration);
            #pragma omp parallel for reduction(max : MaxColumn)
            // NOTE(miha): Iterate all cells.
            for(u32 Row = 0; Row < CurrentGrid->Size; ++Row)
            {
                for(u32 Column = 0; Column < CurrentGrid->Size; ++Column)
                {
                    cell Cell = GridElement(CurrentGrid, Row, Column);
                    // NOTE(miha): Give EDGE cells Beta amount of water.
                    if(Cell.Type == EDGE)
                    {
                        CurrentGrid->Cells[Row*CurrentGrid->Size + Column].Value = Beta;
                        continue;
                    }
                    // NOTE(miha): New water value for the cell.
                    f32 NewWaterValue = 0.0f;
                    if(IsReceptive(Cell))
                    {
                        // NOTE(miha): Cell is receptive, we don't have diffusion.
                        NewWaterValue += Cell.Value;

                        f32 NeighbourDiffusion = 0.0f;
                        for(u32 Direction = 0; Direction < 6; ++Direction)
                        {
                            ivec2 Neighbour = GridNeighbour(Row, Column, Direction);
                            if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid->Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;
                            cell NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            if(!IsReceptive(NeighbourCell))
                            {
                                NeighbourDiffusion += NeighbourCell.Value;
                            }
                        }
                        NeighbourDiffusion /= 6.0f;
                        NewWaterValue += (Alpha/2.0f)*NeighbourDiffusion;
                        NewWaterValue += Gamma;
                    }
                    else
                    {
                        // NOTE(miha): Cell is not receptive, we only have diffusion.
                        NewWaterValue += Cell.Value;
                        f32 NeighbourDiffusion = 0.0f;
                        for(u32 Direction = 0; Direction < 6; ++Direction)
                        {
                            ivec2 Neighbour = GridNeighbour(Row, Column, Direction);
                            if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid->Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;
                            cell NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            if(!IsReceptive(NeighbourCell))
                            {
                                NeighbourDiffusion += NeighbourCell.Value;
                            }
                        }
                        NeighbourDiffusion /= 6.0f;
                        NewWaterValue += (Alpha/2.0f)*(NeighbourDiffusion - Cell.Value);
                    }
                    // NOTE(miha): Update new grid.
                    NextGrid->Cells[Row*NextGrid->Size + Column].Value = NewWaterValue;
                    if(NewWaterValue > 1.0f)
                    {
                        NextGrid->Cells[Row*NextGrid->Size + Column].Type = FROZEN;
                        for(u32 Direction = 0; Direction < 6; ++Direction)
                        {
                            ivec2 Neighbour = GridNeighbour(Row, Column, Direction);
                            if(Neighbour.Row < 0 || Neighbour.Row > NextGrid->Size || Neighbour.Column < 0 || Neighbour.Column > NextGrid->Size)
                                continue;
                            cell NeighbourCell = GridElement(NextGrid, Neighbour.Row, Neighbour.Column);
                            if(NeighbourCell.Type == EDGE)
                            {
                                if(Column > MaxColumn)
                                    MaxColumn = Column;
                                continue;
                            }
                            if(!IsReceptive(NeighbourCell))
                                NextGrid->Cells[Neighbour.Row*NextGrid->Size + Neighbour.Column].Type = BOUNDARY;
                        }
                    }
                }
            }
            // NOTE(miha): Switch grids.
            grid *Temp = NextGrid;
            NextGrid = CurrentGrid;
            CurrentGrid = Temp;
            if(MaxIteration == -1)
            {
                if(MaxColumn == CurrentGrid->Size-2)
                    Running = 0;
            }
            else
            {
                if(Iteration > (u32)MaxIteration)
                    Running = 0;
            }

#if defined(SAVE_DURING_ITERATIONS)
            if(Iteration % SAVE_DURING_ITERATIONS_INTERVAL == 0)
            {
                char FileNameBuffer[256] = {};
                snprintf(FileNameBuffer, 256, "out%d.png", Iteration / SAVE_DURING_ITERATIONS_INTERVAL);
                //PrintGrid(CurrentGrid);
                DrawGrid(CurrentGrid, &Image);
                stbi_write_png(FileNameBuffer, Image.Width, Image.Height, Image.ChannelsPerPixel, Image.Pixels, Image.Width*Image.ChannelsPerPixel);
            }
#endif
            Iteration++;
        }
        f64 TimeEnd = omp_get_wtime();
        printf("time elapsed: %lf\n", TimeEnd - TimeStart);
        DrawGrid(CurrentGrid, &Image);
        stbi_write_png("out_OpenMP.png", Image.Width, Image.Height, Image.ChannelsPerPixel, Image.Pixels, Image.Width*Image.ChannelsPerPixel);
    }
    else
    {
        printf("Failed generating grid\n");
    }
}
