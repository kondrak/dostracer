#include "src/input.h"
#include "src/dt_math.h"
#include "src/dt_trace.h"
#include <conio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __WATCOMC__
#define outportb outp
#endif

/*
 * Raytracer for DOS written in C. 
 * (c) 2015 Krzysztof Kondrak
 * Planes, spheres, reflections and refractions.
 * Uses standard VGA palette and 320x200 mode.
 * Cmd flags: 
 * -d enables basic ordered dither
 * -g grayscale render
 * -f XX set fov to XX, default is 45 degrees
 *
 * Turbo C++ 3.1 ready. Requires large memory model.
 */

extern int VGAPalette[][3];
extern int GrayscalePalette[][3];

static const int SCREEN_WIDTH  = 320;
static const int SCREEN_HEIGHT = 200;

int (*ACTIVE_PALETTE)[3] = VGAPalette;
// cmd line controlled settings
int GRAYSCALE_ON = 0;
int GRAYSCALE_PAL_ON = 0;
int DITHER_ON    = 0;

// pointer to VGA memory
unsigned char *VGA = (unsigned char *)0xA0000000L;

// graphics mode setter
void setMode(unsigned char mode)
{
    _asm {
            mov ah, 0x00
            mov al, mode
            int 10h
    }
}

// good bye
void Shutdown(int exitCode)
{
    setMode(0x03);
    exit(exitCode);
}

/* ***** */

int main(int argc, char **argv)
{
    float invWidth, invHeight, aspectRatio, angle;
    int x, y, pixelColor, renderFinished = 0;
    int fov = 45;
    unsigned short *keysPressed = translateInput();
    Ray eye;
    Scene rtScene;

    for (x = 0; x < argc; x++)
    {
        // grayscale flag
        if(!strcmp(argv[x], "-g"))
            GRAYSCALE_ON = 1;

        // use a full grayscale palette flag - overrides regular grayscale
        if(!strcmp(argv[x], "-gp"))
            GRAYSCALE_PAL_ON = 1;

        // dither flag
        if(!strcmp(argv[x], "-d"))
            DITHER_ON = 1;

        // custom fov flag
        if(!strcmp(argv[x], "-f") && x + 1 < argc)
        {
            if (isdigit(argv[x + 1][0]))
                fov = atof(argv[x + 1]);
        }
    }

    setMode(0x13);

    // generate and set a grayscale palette
    if(GRAYSCALE_PAL_ON)
    {
        ACTIVE_PALETTE = GrayscalePalette;

        outportb(0x03c8, 0);
        for(x = 0; x < 768; ++x)
            outportb(0x03c9, x/3 >> 2);
    }

    /*** Scene setup ***/
    rtScene.spheres[0].m_origin.m_x = -0.05;
    rtScene.spheres[0].m_origin.m_y = -0.38;
    rtScene.spheres[0].m_origin.m_z = -1.5;
    rtScene.spheres[0].m_radius = 0.11;
    rtScene.spheres[0].m_color[0] = 0x00;
    rtScene.spheres[0].m_color[1] = 0xEE;
    rtScene.spheres[0].m_color[2] = 0x00;
    rtScene.spheres[0].m_reflective = 0;
    rtScene.spheres[0].m_refractive = 0;

    rtScene.spheres[1].m_origin.m_x = 0.25;
    rtScene.spheres[1].m_origin.m_y = -0.2;
    rtScene.spheres[1].m_origin.m_z = -1.5;
    rtScene.spheres[1].m_radius = 0.2;
    rtScene.spheres[1].m_color[0] = 0xFF;
    rtScene.spheres[1].m_color[1] = 0xFF;
    rtScene.spheres[1].m_color[2] = 0xFF;
    rtScene.spheres[1].m_reflective = 0;
    rtScene.spheres[1].m_refractive = 0;

    rtScene.spheres[2].m_origin.m_x = -0.2;
    rtScene.spheres[2].m_origin.m_y = -0.15;
    rtScene.spheres[2].m_origin.m_z = -2.0;
    rtScene.spheres[2].m_radius = 0.2;
    rtScene.spheres[2].m_color[0] = 0x00;
    rtScene.spheres[2].m_color[1] = 0x00;
    rtScene.spheres[2].m_color[2] = 0xDD;
    rtScene.spheres[2].m_reflective = 1;
    rtScene.spheres[2].m_refractive = 0;

    rtScene.spheres[3].m_origin.m_x = -0.85;
    rtScene.spheres[3].m_origin.m_y = -0.22;
    rtScene.spheres[3].m_origin.m_z = -3.0;
    rtScene.spheres[3].m_radius = 0.05;
    rtScene.spheres[3].m_color[0] = 0xFF;
    rtScene.spheres[3].m_color[1] = 0x00;
    rtScene.spheres[3].m_color[2] = 0xFF;
    rtScene.spheres[3].m_reflective = 0;
    rtScene.spheres[3].m_refractive = 0;

    rtScene.spheres[4].m_origin.m_x = 0.35;
    rtScene.spheres[4].m_origin.m_y = -0.38;
    rtScene.spheres[4].m_origin.m_z = -1.0;
    rtScene.spheres[4].m_radius = 0.07;
    rtScene.spheres[4].m_color[0] = 0xEE;
    rtScene.spheres[4].m_color[1] = 0x00;
    rtScene.spheres[4].m_color[2] = 0x00;
    rtScene.spheres[4].m_reflective = 1;
    rtScene.spheres[4].m_refractive = 0;

    rtScene.spheres[5].m_origin.m_x = -0.33;
    rtScene.spheres[5].m_origin.m_y = -0.22;
    rtScene.spheres[5].m_origin.m_z = -1.1;
    rtScene.spheres[5].m_radius = 0.1;
    rtScene.spheres[5].m_color[0] = 0xFF;
    rtScene.spheres[5].m_color[1] = 0xFF;
    rtScene.spheres[5].m_color[2] = 0xFF;
    rtScene.spheres[5].m_reflective = 0;
    rtScene.spheres[5].m_refractive = 1;

    // front wall
    rtScene.planes[0].m_normal.m_x = 0;
    rtScene.planes[0].m_normal.m_y = 0;
    rtScene.planes[0].m_normal.m_z = -1;
    rtScene.planes[0].m_distance = -5;
    rtScene.planes[0].m_reflective = 0;
    rtScene.planes[0].m_color[0] = 0xFF;
    rtScene.planes[0].m_color[1] = 0xFF;
    rtScene.planes[0].m_color[2] = 0xFF;

    // bottom wall (mirror)
    rtScene.planes[1].m_normal.m_x = 0;
    rtScene.planes[1].m_normal.m_y = 1;
    rtScene.planes[1].m_normal.m_z = 0;
    rtScene.planes[1].m_distance = 0.5;
    rtScene.planes[1].m_reflective = 1;
    rtScene.planes[1].m_color[0] = 0x00;
    rtScene.planes[1].m_color[1] = 0x00;
    rtScene.planes[1].m_color[2] = 0x00;

    // left wall
    rtScene.planes[2].m_normal.m_x = 1;
    rtScene.planes[2].m_normal.m_y = 0;
    rtScene.planes[2].m_normal.m_z = 0;
    rtScene.planes[2].m_distance = 1.2;
    rtScene.planes[2].m_reflective = 0;
    rtScene.planes[2].m_color[0] = 0xFF;
    rtScene.planes[2].m_color[1] = 0x00;
    rtScene.planes[2].m_color[2] = 0x00;

    // right wall
    rtScene.planes[3].m_normal.m_x = -1;
    rtScene.planes[3].m_normal.m_y = 0;
    rtScene.planes[3].m_normal.m_z = 0;
    rtScene.planes[3].m_distance = 1.2;
    rtScene.planes[3].m_reflective = 0;
    rtScene.planes[3].m_color[0] = 0x00;
    rtScene.planes[3].m_color[1] = 0xFF;
    rtScene.planes[3].m_color[2] = 0x00;

    // back wall (behind camera)
    rtScene.planes[4].m_normal.m_x = 0;
    rtScene.planes[4].m_normal.m_y = 0;
    rtScene.planes[4].m_normal.m_z = 1;
    rtScene.planes[4].m_distance = -0.2;
    rtScene.planes[4].m_reflective = 0;
    rtScene.planes[4].m_color[0] = 0x00;
    rtScene.planes[4].m_color[1] = 0x00;
    rtScene.planes[4].m_color[2] = 0x00;

    // ceiling
    rtScene.planes[5].m_normal.m_x = 0;
    rtScene.planes[5].m_normal.m_y = -1;
    rtScene.planes[5].m_normal.m_z = 0;
    rtScene.planes[5].m_distance = 0.5;
    rtScene.planes[5].m_reflective = 0;
    rtScene.planes[5].m_color[0] = 0xFF;
    rtScene.planes[5].m_color[1] = 0xFF;
    rtScene.planes[5].m_color[2] = 0xFF;

    // place the light in the upper-left corner
    rtScene.lightPos.m_x = -0.7;
    rtScene.lightPos.m_y = 1.5;
    rtScene.lightPos.m_z = 0.0;

    // place the eye at the origin for simplicity
    eye.m_origin.m_x = 0.0;
    eye.m_origin.m_y = 0.0;
    eye.m_origin.m_z = 0.0;

    // calculate the view
    invWidth = 1.0f / SCREEN_WIDTH;
    invHeight = 1.0f / SCREEN_HEIGHT;
    aspectRatio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    angle = tan(M_PI * 0.5 * fov / 180.);

    while (!keysPressed[KEY_ESC])
    {
        // perform draw after vsync
        if (!renderFinished)
        {
            for (x = 0; x < SCREEN_WIDTH; x++)
            {
                for (y = 0; y < SCREEN_HEIGHT; y++)
                {
                    eye.m_dir.m_x = (2 * ((x + 0.5) * invWidth) - 1) * angle * aspectRatio;
                    eye.m_dir.m_y = (1 - 2 * ((y + 0.5) * invHeight)) * angle;
                    eye.m_dir.m_z = -1.0;
                    normalize(&eye.m_dir);

                    pixelColor = rayTrace(&eye, &rtScene, NULL, x, y);

                    // draw the pixel
                    VGA[(y << 8) + (y << 6) + x] = pixelColor;

                    // process input mid-render so that we can quit before image is complete
                    keysPressed = translateInput();
                    if (keysPressed[KEY_ESC])
                        Shutdown(0);
                }
            }
            renderFinished = 1;
        }
        else
        {
            keysPressed = translateInput();
        }
    }

    Shutdown(0);
    return 0;
}
