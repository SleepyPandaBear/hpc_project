#include "math.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "snow_crystal_growth_model.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "snow_crystal_growth_renderer.h"

// TODO(miha): Grid
// TODO(miha): Save image
// TODO(miha): Actual model
// TODO(miha): Possible header files: grid.h

// NOTE(miha): For grid we will use offset coordinates.

b32
GenerateGrid(grid *Grid)
{
    // NOTE(miha): Grid has to be odd sized.
    if(Grid->Size % 2 == 0)
        return 0;

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
        printf("bottom: row_idx: %d\n", RowIndex);
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
        printf("right: col_idx: %d\n", ColumnIndex);
        Cells[(Grid->Size-1) + ColumnIndex*Grid->Size].Type = EDGE;
    }

    // TODO(miha): Set middle cell to FROZEN
    Cells[(Grid->Size/2)*Grid->Size + (Grid->Size/2)].Type = FROZEN;

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
                }
                else
                {
                    ivec2 Neighbour = GridNeighbour(Row, Column, N);
                    printf("%6.2f ", GridElement(Grid, Neighbour.X, Neighbour.Y).Value);
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

i32
main(i32 ArgumentCount, char *ArgumentValues[])
{
    grid Grid = {};
    // CARE(miha): CellSize is the radius of a cell!
    Grid.CellSize = 10;
    Grid.Size = 21;
    if(GenerateGrid(&Grid))
    {
        image Image = {};
        Image.ToPixelMultiplier = Grid.CellSize;
        Image.ChannelsPerPixel = 3;
        Image.Width = Grid.Size * 1.5f * Image.ToPixelMultiplier;
        Image.Height = Grid.Size * sqrtf(3) * Image.ToPixelMultiplier;
        u8 *ImagePixels = (u8 *)malloc(Image.Width*Image.Height*Image.ChannelsPerPixel);
        Image.Pixels = ImagePixels;

        DrawGrid(&Grid, &Image);

        stbi_write_png("out.png", Image.Width, Image.Height, Image.ChannelsPerPixel, Image.Pixels, Image.Width*Image.ChannelsPerPixel);
    }
    else
    {
        printf("Failed generating grid\n");
    }
}
