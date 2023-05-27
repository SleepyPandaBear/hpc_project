#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"
#include "snow_crystal_growth_model.h"

#include "mpi.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define DRAW_CELL_BORDER
#include "snow_crystal_growth_renderer.h"

//#define SAVE_DURING_ITERATIONS
#define SAVE_DURING_ITERATIONS_INTERVAL (20)

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

#if 1
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

#if 1
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

i32
main(i32 ArgumentCount, char *ArgumentValues[])
{
    f32 Alpha = atof(ArgumentValues[1]);
    f32 Beta = atof(ArgumentValues[2]);
    f32 Gamma = atof(ArgumentValues[3]);

    i32 Rank;
    i32 NumberOfProcesses;
    u32 Sender;
    u32 Receiver;
    u32 Tag = 0;

    MPI_Init(&ArgumentCount, &ArgumentValues);
    MPI_Comm_rank(MPI_COMM_WORLD, &Rank);
    MPI_Comm_size(MPI_COMM_WORLD, &NumberOfProcesses);

    // NOTE(miha): Create custom new MPI data type for 'cell' called 'mpi_cell'.
    i32 mpiCellBlocks[2] = {1, 1};
    MPI_Aint mpiCellDisplacements[2] = {offsetof(cell, Type), offsetof(cell, Value)};
    MPI_Datatype mpiCellInputType[2] = {MPI_CHAR, MPI_FLOAT};
    MPI_Datatype mpi_cell;
    MPI_Type_create_struct(2, mpiCellBlocks, mpiCellDisplacements, mpiCellInputType, &mpi_cell);
    MPI_Type_commit(&mpi_cell);

    grid Grid = {};
    Grid.Size = 21;
    Grid.CellSize = 10;

    // NOTE(miha): Initialize grid on process 0.
    if(Rank == 0)
    {
        GenerateGrid(&Grid, Beta);
        FillGridWithTestElements(&Grid);
        //PrintGrid(&Grid);
    }

    // NOTE(miha): Calculate how much work does each process do.
    u32 NumberOfRows = Grid.Size/NumberOfProcesses;
    u32 NumberOfRowsRoot = NumberOfRows + Grid.Size%NumberOfProcesses;
    // NOTE(miha): For MPI_Scatterv
    i32 SendCount[NumberOfProcesses] = {};
    i32 Displacement[NumberOfProcesses] = {};
    for(u32 I = 0; I < NumberOfProcesses; ++I)
    {
        SendCount[I] = NumberOfRows * Grid.Size;
    }
    SendCount[0] = NumberOfRowsRoot * Grid.Size;
    Displacement[0] = 0;
    for(u32 I = 1; I < NumberOfProcesses; ++I)
    {
        if(I == 1)
        {
            Displacement[I] = NumberOfRowsRoot * Grid.Size;
        }
        else
        {
            Displacement[I] = (I-1)*(NumberOfRows*Grid.Size) + (NumberOfRowsRoot*Grid.Size);
        }
    }

    // NOTE(miha): Only a part of the grid - each process does calcualtions on
    // this part.
    grid LocalGrid = {};
    LocalGrid.Size = Grid.Size;
    LocalGrid.CellSize = Grid.CellSize;
    grid LocalGridNext = {};
    LocalGridNext.Size = Grid.Size;
    LocalGridNext.CellSize = Grid.CellSize;

    // NOTE(miha): Init local grid and next local grid - part of the grid for computtation.
    if(Rank == 0)
    {
        LocalGrid.Cells = (cell *)malloc(NumberOfRowsRoot*LocalGrid.Size*sizeof(cell));
        LocalGridNext.Cells = (cell *)malloc(NumberOfRowsRoot*LocalGridNext.Size*sizeof(cell));
    }
    else
    {
        LocalGrid.Cells = (cell *)malloc(NumberOfRows*LocalGrid.Size*sizeof(cell));
        LocalGridNext.Cells = (cell *)malloc(NumberOfRows*LocalGridNext.Size*sizeof(cell));
    }

    // NOTE(miha): Allocate memory for neighbouring rows, get this rows every iteration.
    cell *NeighbourRowTop = (cell *)malloc(LocalGrid.Size*sizeof(cell));
    cell *NeighbourRowBottom = (cell *)malloc(LocalGrid.Size*sizeof(cell));

    MPI_Scatterv(Grid.Cells, SendCount, Displacement, mpi_cell, LocalGrid.Cells, SendCount[Rank], mpi_cell, 0, MPI_COMM_WORLD);

    b32 Running = 1;
    u32 Iteration = 0;

    grid *CurrentGrid = &LocalGrid;
    grid *NextGrid = &LocalGridNext;

    while(Running)
    {
        // NOTE(miha): Receive top row from neighbour.
        if(Rank == 0)
        {
            MPI_Sendrecv(&CurrentGrid->Cells[(NumberOfRowsRoot-1)*LocalGrid.Size-1], LocalGrid.Size, mpi_cell, (Rank + 1) % NumberOfProcesses, 0,
                        NeighbourRowTop, LocalGrid.Size, mpi_cell, (Rank + NumberOfProcesses - 1) % NumberOfProcesses,
                        0, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
        }
        else
        {
            MPI_Sendrecv(&CurrentGrid->Cells[(NumberOfRows-1)*LocalGrid.Size-1], LocalGrid.Size, mpi_cell, (Rank + 1) % NumberOfProcesses, 0,
                        NeighbourRowTop, LocalGrid.Size, mpi_cell, (Rank + NumberOfProcesses - 1) % NumberOfProcesses,
                        0, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);
        }

        // NOTE(miha): Receive bottom row from neighbour.
        MPI_Sendrecv(CurrentGrid->Cells, LocalGrid.Size, mpi_cell, (Rank + NumberOfProcesses - 1) % NumberOfProcesses, 0,
                     NeighbourRowBottom, LocalGrid.Size, mpi_cell, (Rank + 1) % NumberOfProcesses,
                     0, MPI_COMM_WORLD, MPI_STATUSES_IGNORE);

        // NOTE(miha): Iterate all cells.
        u32 MaxColumn = 0;
        if(Rank == 0)
        {
            // NOTE(miha): Root process has different amount of rows than other processes.
            for(u32 Row = 0; Row < NumberOfRowsRoot; ++Row)
            {
                for(u32 Column = 0; Column < CurrentGrid->Size; ++Column)
                {
#if 0
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
                            if(Neighbour.Row < 0 || Neighbour.Row > NumberOfRowsRoot + 1 || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell;
                            if(Neighbour.Row == NumberOfRowsRoot + 1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the bottom.
                                NeighbourCell = NeighbourRowBottom[Neighbour.Column];
                            }
                            else
                            {
                                NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            }

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
                            if(Neighbour.Row < 0 || Neighbour.Row > NumberOfRowsRoot + 1 || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell;
                            if(Neighbour.Row == NumberOfRowsRoot + 1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the bottom.
                                NeighbourCell = NeighbourRowBottom[Neighbour.Column];
                            }
                            else
                            {
                                NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            }

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
                            if(Neighbour.Row < 0 || Neighbour.Row > NumberOfRowsRoot + 1 || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
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
#endif
                }
            }
        }
        else
        {
            // NOTE(miha): Non root processes.
            for(u32 Row = 0; Row < NumberOfRows; ++Row)
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
                            if(Neighbour.Row < -1 || Neighbour.Row > NumberOfRows + 1 || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell;
                            if(Neighbour.Row == -1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the top.
                                NeighbourCell = NeighbourRowTop[Neighbour.Column];
                            }
                            else if(Neighbour.Row == NumberOfRows + 1 && Rank != NumberOfProcesses-1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the bottom.
                                NeighbourCell = NeighbourRowBottom[Neighbour.Column];
                            }
                            else
                            {
                                NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            }

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
                            if(Neighbour.Row < -1 || Neighbour.Row > NumberOfRows + 1 || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell;
                            if(Neighbour.Row == -1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the top.
                                NeighbourCell = NeighbourRowTop[Neighbour.Column];
                            }
                            else if(Neighbour.Row == NumberOfRows + 1 && Rank != NumberOfProcesses-1)
                            {
                                // NOTE(miha): Cell is on the neighbor at the bottom.
                                NeighbourCell = NeighbourRowBottom[Neighbour.Column];
                            }
                            else
                            {
                                NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
                            }

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
                            if(Neighbour.Row < 0 || Neighbour.Row > NumberOfRows || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid->Size)
                                continue;

                            cell NeighbourCell = GridElement(CurrentGrid, Neighbour.Row, Neighbour.Column);
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
        }
        // NOTE(miha): Switch grids.
        grid *Temp = NextGrid;
        NextGrid = CurrentGrid;
        CurrentGrid = Temp;

        /*
        if(!FromIterations && MaxColumn == CurrentGrid->Size-2)
        {
            FromIterations = Iteration;
            Running = 0;
        }
        */
        //if(FromIterations && Iteration - FromIterations > 20)
        //    Running = 0;
        if(Iteration > 100)
        {
            if(Rank == 0)
                printf("someting is happening\n");
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
    printf("my rank is: %d, my rows: %d\n", Rank, SendCount[Rank]);

#if 0
#endif

    //MPI_Barrier(MPI_COMM_WORLD);
    MPI_Gatherv(CurrentGrid->Cells, SendCount[Rank], mpi_cell, Grid.Cells, SendCount, Displacement, mpi_cell, 0, MPI_COMM_WORLD);

    if(0 && Rank == 0)
    {
        image Image = {};
        Image.ToPixelMultiplier = Grid.CellSize;
        Image.ChannelsPerPixel = 3;
        Image.Width = Grid.Size * 1.5f * Image.ToPixelMultiplier;
        Image.Height = Grid.Size * sqrtf(3) * Image.ToPixelMultiplier;
        u8 *ImagePixels = (u8 *)malloc(Image.Width*Image.Height*Image.ChannelsPerPixel);
        Image.Pixels = ImagePixels;
        DrawGrid(CurrentGrid, &Image);
        stbi_write_png("out_basic_openmpi.png", Image.Width, Image.Height, Image.ChannelsPerPixel, Image.Pixels, Image.Width*Image.ChannelsPerPixel);
    }

    MPI_Type_free(&mpi_cell);
    MPI_Finalize();
}
