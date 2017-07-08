#ifndef DT_TRACE_H
#define DT_TRACE_H

#include "src/dt_math.h"

/*
 * core raytracing structs and functions
 */

typedef struct
{
    Vector3f m_origin;
    Vector3f m_dir;
} Ray;

typedef struct
{
    Vector3f m_origin;
    double  m_radius;
    int     m_reflective; //  1/0 
    int     m_refractive; //  1/0 
    double  m_color[3];
} Sphere;

typedef struct
{
    Vector3f m_normal;
    double   m_distance;
    int      m_reflective; //  1/0 
    double   m_color[3];
} Plane;

#define NUM_SPHERES 6
#define NUM_PLANES 6
typedef struct
{
    Sphere spheres[NUM_SPHERES];
    Plane  planes[NUM_PLANES];
    Vector3f lightPos;
} Scene;

/* ***** */

#ifdef __cplusplus
extern "C" {
#endif

    // intersection and ray propagation
    Vector3f reflect(const Ray *r, const Vector3f *normal);
    Vector3f refract(const Ray *r, const Vector3f *normal);
    double intersectSphere(const Ray *r, const Sphere *s, Vector3f *oIntersectPoint);
    double intersectPlane(const Ray *r, const Plane *p, Vector3f *oIntersectPoint);

    // raytracing
    int rayTrace(const Ray *r, const Scene *s, const void *currObject, int x, int y);

    // shading and color determination
    int  orderedDither(const double *pixelColor, int x, int y);
    void lambertShade(const Vector3f *light, const Vector3f *normal, const double *iRGB, double *oRGB);
    int  findColor(const double *srcColor);

#ifdef __cplusplus
}
#endif
#endif
