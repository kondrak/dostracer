#ifndef DT_MATH_H
#define DT_MATH_H

/*
 * necessary math for the raytracer
 */

#ifndef M_PI
#define M_PI 3.141592654
#endif

typedef struct
{
    double m_x;
    double m_y;
    double m_z;
} Vector3f;

#ifdef __cplusplus
extern "C" {
#endif

    double dotProduct(const Vector3f *v1, const Vector3f *v2);
    double invLength(const Vector3f *v);
    void   normalize(Vector3f *v);
    void   clamp(double *color);

#ifdef __cplusplus
}
#endif
#endif
