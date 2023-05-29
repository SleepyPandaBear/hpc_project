#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "snow_crystal_growth_model.h"

// NOTE(miha): Imprt CUDA related libraries.
#include "cuda.h"
#include "cuda_runtime.h"
#include "helper_cuda.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

//#define DRAW_CELL_BORDER
#include "snow_crystal_growth_renderer.h"

//#define SAVE_DURING_ITERATIONS
#define SAVE_DURING_ITERATIONS_INTERVAL (50)

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
b32
IsReceptive(cell Cell)
{
    if(Cell.Type == FROZEN || Cell.Type == BOUNDARY)
        return 1;
    return 0;
}
#endif

__forceinline__ __device__ b32
gpuIsReceptive(cell Cell)
{
    if(Cell.Type == FROZEN || Cell.Type == BOUNDARY)
        return 1;
    return 0;
}

__device__ ivec2
gpuGridNeighbour(u32 Row, u32 Column, u32 Direction)
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

__global__ void
IterationGPUShared(grid CurrentGrid, grid NextGrid, u32 *MaxColumn, f32 *Alpha, f32 *Beta, f32 *Gamma)
{
    u32 LocalID = threadIdx.x;
    u32 GlobalID = blockIdx.x * blockDim.x + threadIdx.x;
    u32 Row = GlobalID / CurrentGrid.Size;
    u32 Column = GlobalID % CurrentGrid.Size;

    // NOTE(miha): BlockSize = 256
    // If Cell.Type != EDGE we can init values?
    __shared__ cell LocalCells[256 * 3 + 4];

    LocalCells[LocalID] = CurrentGrid.Cells[GlobalID];
    LocalCells[LocalID] = CurrentGrid.Cells[GlobalID];

    if(GlobalID < CurrentGrid.Size*CurrentGrid.Size)
    {
        cell Cell = CurrentGrid.Cells[GlobalID];

        if(Cell.Type == EDGE)
        {
            CurrentGrid.Cells[GlobalID].Value = *Beta;
        }
        else
        {
            f32 NewWaterValue = 0.0f;
            if(gpuIsReceptive(Cell))
            {
                NewWaterValue += Cell.Value;

                f32 NeighbourDiffusion = 0.0f;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = CurrentGrid.Cells[Neighbour.Row * CurrentGrid.Size + Neighbour.Column];
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NeighbourDiffusion += NeighbourCell.Value;
                        }
                    }
                }
                NeighbourDiffusion /= 6.0f;
                NewWaterValue += (*Alpha/2.0f)*NeighbourDiffusion;
                NewWaterValue += *Gamma;
            }
            else
            {
                NewWaterValue += Cell.Value;
                f32 NeighbourDiffusion = 0.0f;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = CurrentGrid.Cells[Neighbour.Row * CurrentGrid.Size + Neighbour.Column];
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NeighbourDiffusion += NeighbourCell.Value;
                        }
                    }
                }
                NeighbourDiffusion /= 6.0f;
                NewWaterValue += (*Alpha/2.0f)*(NeighbourDiffusion - Cell.Value);
            }

            NextGrid.Cells[GlobalID].Value = NewWaterValue;
            if(NewWaterValue > 1.0f)
            {
                NextGrid.Cells[GlobalID].Type = FROZEN;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = NextGrid.Cells[Neighbour.Row * NextGrid.Size + Neighbour.Column];
                        if(NeighbourCell.Type == EDGE)
                        {
                            if(Column > *MaxColumn)
                                *MaxColumn = Column;
                        }
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NextGrid.Cells[Neighbour.Row * NextGrid.Size + Neighbour.Column].Type = BOUNDARY;
                        }
                    }
                }

            }
        }
    }
}

__global__ void
IterationGPU(grid CurrentGrid, grid NextGrid, u32 *MaxColumn, f32 *Alpha, f32 *Beta, f32 *Gamma)
{
    u32 LocalID = threadIdx.x;
    u32 GlobalID = blockIdx.x * blockDim.x + threadIdx.x;
    u32 Row = GlobalID / CurrentGrid.Size;
    u32 Column = GlobalID % CurrentGrid.Size;

    if(GlobalID < CurrentGrid.Size*CurrentGrid.Size)
    {
        cell Cell = CurrentGrid.Cells[GlobalID];

        if(Cell.Type == EDGE)
        {
            CurrentGrid.Cells[GlobalID].Value = *Beta;
        }
        else
        {
            f32 NewWaterValue = 0.0f;
            if(gpuIsReceptive(Cell))
            {
                NewWaterValue += Cell.Value;

                f32 NeighbourDiffusion = 0.0f;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = CurrentGrid.Cells[Neighbour.Row * CurrentGrid.Size + Neighbour.Column];
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NeighbourDiffusion += NeighbourCell.Value;
                        }
                    }
                }
                NeighbourDiffusion /= 6.0f;
                NewWaterValue += (*Alpha/2.0f)*NeighbourDiffusion;
                NewWaterValue += *Gamma;
            }
            else
            {
                NewWaterValue += Cell.Value;
                f32 NeighbourDiffusion = 0.0f;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = CurrentGrid.Cells[Neighbour.Row * CurrentGrid.Size + Neighbour.Column];
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NeighbourDiffusion += NeighbourCell.Value;
                        }
                    }
                }
                NeighbourDiffusion /= 6.0f;
                NewWaterValue += (*Alpha/2.0f)*(NeighbourDiffusion - Cell.Value);
            }

            NextGrid.Cells[GlobalID].Value = NewWaterValue;
            if(NewWaterValue > 1.0f)
            {
                NextGrid.Cells[GlobalID].Type = FROZEN;
                for(u32 Direction = 0; Direction < 6; ++Direction)
                {
                    ivec2 Neighbour = gpuGridNeighbour(Row, Column, Direction);
                    if(Neighbour.Row < 0 || Neighbour.Row > CurrentGrid.Size || Neighbour.Column < 0 || Neighbour.Column > CurrentGrid.Size)
                    {
                        // NOTE(miha): Out of bounds, ignore it.
                    }
                    else
                    {
                        cell NeighbourCell = NextGrid.Cells[Neighbour.Row * NextGrid.Size + Neighbour.Column];
                        if(NeighbourCell.Type == EDGE)
                        {
                            if(Column > *MaxColumn)
                                *MaxColumn = Column;
                        }
                        if(!gpuIsReceptive(NeighbourCell))
                        {
                            NextGrid.Cells[Neighbour.Row * NextGrid.Size + Neighbour.Column].Type = BOUNDARY;
                        }
                    }
                }

            }
        }
    }
}

// NOTE(miha): We generate image with CPU and copy it to the GPU.

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

        u32 BlockSize = 256;
        u32 GridSize = ((Grid.Size*Grid.Size)/BlockSize) + 1;

        cell *gpuCurrentGridCells;
        checkCudaErrors(cudaMalloc(&gpuCurrentGridCells, Grid.Size*Grid.Size*sizeof(cell)));
        checkCudaErrors(cudaMemcpy(gpuCurrentGridCells, CurrentGrid->Cells, Grid.Size*Grid.Size*sizeof(cell), cudaMemcpyHostToDevice));
        CurrentGrid->Cells = gpuCurrentGridCells;

        cell *gpuNextGridCells;
        checkCudaErrors(cudaMalloc(&gpuNextGridCells, NewGrid.Size*NewGrid.Size*sizeof(cell)));
        checkCudaErrors(cudaMemcpy(gpuNextGridCells, NextGrid->Cells, NewGrid.Size*NewGrid.Size*sizeof(cell), cudaMemcpyHostToDevice));
        NextGrid->Cells = gpuNextGridCells;

        u32 *gpuMaxColumn;
        checkCudaErrors(cudaMalloc(&gpuMaxColumn, sizeof(u32)));
        checkCudaErrors(cudaMemset(gpuMaxColumn, 0, sizeof(u32)));

        f32 *gpuAlpha;
        checkCudaErrors(cudaMalloc(&gpuAlpha, sizeof(f32)));
        checkCudaErrors(cudaMemcpy(gpuAlpha, &Alpha, sizeof(f32), cudaMemcpyHostToDevice));

        f32 *gpuBeta;
        checkCudaErrors(cudaMalloc(&gpuBeta, sizeof(f32)));
        checkCudaErrors(cudaMemcpy(gpuBeta, &Beta, sizeof(f32), cudaMemcpyHostToDevice));

        f32 *gpuGamma;
        checkCudaErrors(cudaMalloc(&gpuGamma, sizeof(f32)));
        checkCudaErrors(cudaMemcpy(gpuGamma, &Gamma, sizeof(f32), cudaMemcpyHostToDevice));

        cudaEvent_t Start, Stop;
        cudaEventCreate(&Start);
        cudaEventCreate(&Stop);
        cudaEventRecord(Start);

        while(Running)
        {
            printf("iteration: %d\n", Iteration);
            u32 MaxColumn = 0;

            // TODO(miha): Call CUDA to calculate one iteration.
            // have to pass poiter to current grid on gpu, pointer to next grid on gpu, grid size
            IterationGPU<<<GridSize, BlockSize>>>(*CurrentGrid, *NextGrid, gpuMaxColumn, gpuAlpha, gpuBeta, gpuGamma);
            cudaDeviceSynchronize();
            getLastCudaError("HistogramGPU() execution failed\n");
            checkCudaErrors(cudaMemcpy(&MaxColumn, gpuMaxColumn, sizeof(u32), cudaMemcpyDeviceToHost));

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
            //if(FromIterations && Iteration - FromIterations > 20)
            //    Running = 0;
            //if(Iteration > 1000)
            //    Running = 0;
            //Running = 0;

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

        cudaEventRecord(Stop);
        cudaEventSynchronize(Stop);

        f32 Milliseconds = 0;
        cudaEventElapsedTime(&Milliseconds, Start, Stop);
        printf("Time: %0.3f milliseconds \n", Milliseconds);

        cell *Cells = (cell *)malloc(Grid.Size*Grid.Size*sizeof(cell));
        memset(Cells, 0, Grid.Size*Grid.Size*sizeof(cell));
        checkCudaErrors(cudaMemcpy(Cells, gpuCurrentGridCells, Grid.Size*Grid.Size*sizeof(cell), cudaMemcpyDeviceToHost));
        CurrentGrid->Cells = Cells;
        DrawGrid(CurrentGrid, &Image);
        stbi_write_png("out_cuda.png", Image.Width, Image.Height, Image.ChannelsPerPixel, Image.Pixels, Image.Width*Image.ChannelsPerPixel);
    }
    else
    {
    }
}
