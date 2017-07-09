#include "src/dt_trace.h"
#include <math.h>
#include <stdlib.h>

extern int (*ACTIVE_PALETTE)[3];

// arbitrary values used for shading and refraction
static const double AMBIENT       = 0.3;
static const double DIFFUSE       = 0.8;
static const double AIR_REFRACT   = 1.0;
static const double GLASS_REFRACT = 2.0;

extern int GRAYSCALE_ON;
extern int GRAYSCALE_PAL_ON;
extern int DITHER_ON;

// threshold map for ordered dithering
#define d(x) x/4.0
double thresholdMap[8][8] = {{ d(1.0),  d(49.0), d(13.0), d(61.0), d(4.0),  d(52.0), d(16.0), d(64.0) },
                             { d(33.0), d(17.0), d(45.0), d(29.0), d(36.0), d(20.0), d(48.0), d(32.0) },
                             { d(9.0),  d(57.0), d(5.0),  d(53.0), d(12.0), d(60.0), d(8.0),  d(56.0) },
                             { d(41.0), d(25.0), d(37.0), d(21.0), d(44.0), d(28.0), d(40.0), d(24.0) },
                             { d(3.0),  d(51.0), d(15.0), d(63.0), d(2.0),  d(50.0), d(14.0), d(62.0) },
                             { d(35.0), d(19.0), d(47.0), d(31.0), d(34.0), d(18.0), d(46.0), d(30.0) },
                             { d(11.0), d(59.0), d(7.0),  d(55.0), d(10.0), d(58.0), d(6.0),  d(54.0) },
                             { d(43.0), d(27.0), d(39.0), d(23.0), d(42.0), d(26.0), d(38.0), d(22.0) }};

// get direction of reflected ray
Vector3f reflect(const Ray *r, const Vector3f *normal)
{
    Vector3f reflectDir;
    double c1 = -dotProduct(normal, &r->m_dir);

    reflectDir.m_x = r->m_dir.m_x + (2 * normal->m_x * c1);
    reflectDir.m_y = r->m_dir.m_y + (2 * normal->m_y * c1);
    reflectDir.m_z = r->m_dir.m_z + (2 * normal->m_z * c1);

    return reflectDir;
}

// get direction of refracted ray
Vector3f refract(const Ray *r, const Vector3f *normal)
{
    Vector3f refractDir;

    double n1 = AIR_REFRACT;   // refraction of original medium
    double n2 = GLASS_REFRACT; // refraction of new medium
    double c1 = -dotProduct(normal, &r->m_dir);
    double n = n1 / n2;
    double c2 = sqrt(1 - n * n * (1 - c1 * c1));

    refractDir.m_x = (n * r->m_dir.m_x) + (n * c1 - c2) * normal->m_x;
    refractDir.m_y = (n * r->m_dir.m_y) + (n * c1 - c2) * normal->m_y;
    refractDir.m_z = (n * r->m_dir.m_z) + (n * c1 - c2) * normal->m_z;

    return refractDir;
}

// sphere intersection
double intersectSphere(const Ray *r, const Sphere *s, Vector3f *oIntersectPoint)
{
    double d, disc;
    double v;
    Vector3f ro;
    ro.m_x = s->m_origin.m_x - r->m_origin.m_x;
    ro.m_y = s->m_origin.m_y - r->m_origin.m_y;
    ro.m_z = s->m_origin.m_z - r->m_origin.m_z;

    v = dotProduct(&ro, &r->m_dir);
    disc = s->m_radius * s->m_radius - (dotProduct(&ro, &ro) - v * v);

    // no intersection
    if (disc < 0.0)
        return -1.0;

    d = sqrt(disc);
    oIntersectPoint->m_x = r->m_origin.m_x + (v - d) * r->m_dir.m_x;
    oIntersectPoint->m_y = r->m_origin.m_y + (v - d) * r->m_dir.m_y;
    oIntersectPoint->m_z = r->m_origin.m_z + (v - d) * r->m_dir.m_z;

    return v - d;
}

// plane intersection
double intersectPlane(const Ray *r, const Plane *p, Vector3f *oIntersectPoint)
{
    double a, b, t;
    b = dotProduct(&p->m_normal, &r->m_dir);

    // normal and eye dir are parallel - no intersection
    if (b == 0.0)
        return -1.0;

    a = dotProduct(&p->m_normal, &r->m_origin) + p->m_distance;
    t = -a / b;

    // no intersection
    if (t < 0.0)
        return -1.0;

    oIntersectPoint->m_x = r->m_origin.m_x + r->m_dir.m_x * t;
    oIntersectPoint->m_y = r->m_origin.m_y + r->m_dir.m_y * t;
    oIntersectPoint->m_z = r->m_origin.m_z + r->m_dir.m_z * t;

    return t;
}

// the actual raytracing
int rayTrace(const Ray *r, const Scene *s, const void *currObject, int x, int y)
{
    int i, pointColor = 0;
    double d, currMinDist = 9999;
    Vector3f p, pp;

    /*** process spheres ***/
    for (i = 0; i < NUM_SPHERES; i++)
    {
        // skip if trying to check against oneself
        if (&s->spheres[i] == (Sphere *)currObject)
            continue;

        d = intersectSphere(r, &s->spheres[i], &p);

        // skip if no intersection
        if (d < 0.0)
            continue;

        if (d < currMinDist)
        {
            double sColor[3];
            Vector3f lVector; // light position in object space for proper shading
            Vector3f sNormal; // sphere normal at intersection point

            lVector.m_x = s->lightPos.m_x - p.m_x;
            lVector.m_y = s->lightPos.m_y - p.m_y;
            lVector.m_z = s->lightPos.m_z - p.m_z;

            sNormal.m_x = -(s->spheres[i].m_origin.m_x - p.m_x);
            sNormal.m_y = -(s->spheres[i].m_origin.m_y - p.m_y);
            sNormal.m_z = -(s->spheres[i].m_origin.m_z - p.m_z);

            normalize(&sNormal);

            currMinDist = d;
            lambertShade(&lVector, &sNormal, s->spheres[i].m_color, sColor);
            pointColor = DITHER_ON ? orderedDither(sColor, x, y) : findColor(sColor);

            if (s->spheres[i].m_reflective != 0)
            {
                Ray rl;
                int reflectColor;

                rl.m_dir = reflect(r, &sNormal);
                rl.m_origin.m_x = p.m_x;
                rl.m_origin.m_y = p.m_y;
                rl.m_origin.m_z = p.m_z;
                reflectColor = rayTrace(&rl, s, &s->spheres[i], x, y);

                // perfect reflection - don't add colors
                sColor[0] = (double)ACTIVE_PALETTE[reflectColor][0];
                sColor[1] = (double)ACTIVE_PALETTE[reflectColor][1];
                sColor[2] = (double)ACTIVE_PALETTE[reflectColor][2];
                clamp(sColor);

                pointColor = DITHER_ON ? orderedDither(sColor, x, y) : findColor(sColor);
            }

            if (s->spheres[i].m_refractive != 0)
            {
                Ray rr;
                int refractColor;

                rr.m_dir = refract(r, &sNormal);
                rr.m_origin.m_x = p.m_x;
                rr.m_origin.m_y = p.m_y;
                rr.m_origin.m_z = p.m_z;
                refractColor = rayTrace(&rr, s, &s->spheres[i], x, y);

                // make the refracted color 90% bright
                sColor[0] = 0.90 * (double)ACTIVE_PALETTE[refractColor][0];
                sColor[1] = 0.90 * (double)ACTIVE_PALETTE[refractColor][1];
                sColor[2] = 0.90 * (double)ACTIVE_PALETTE[refractColor][2];

                pointColor = DITHER_ON ? orderedDither(sColor, x, y) : findColor(sColor);
            }
        }
    }

    /*** process planes ***/
    for (i = 0; i < NUM_PLANES; i++)
    {
        // skip if trying to check against oneself
        if (&s->planes[i] == (Plane *)currObject)
            continue;

        d = intersectPlane(r, &s->planes[i], &pp);

        // skip if no intersection
        if (d < 0.0)
            continue;

        if (d < currMinDist)
        {
            double pColor[3];
            currMinDist = d;

            if (s->planes[i].m_reflective != 0)
            {
                Ray rlp;
                int reflectColor;

                rlp.m_dir = reflect(r, &s->planes[i].m_normal);
                rlp.m_origin.m_x = pp.m_x;
                rlp.m_origin.m_y = pp.m_y;
                rlp.m_origin.m_z = pp.m_z;
                reflectColor = rayTrace(&rlp, s, &s->planes[i], x, y);

                // make reflected object 70% bright
                pColor[0] = 0.70 * (double)ACTIVE_PALETTE[reflectColor][0];
                pColor[1] = 0.70 * (double)ACTIVE_PALETTE[reflectColor][1];
                pColor[2] = 0.70 * (double)ACTIVE_PALETTE[reflectColor][2];

                pointColor = DITHER_ON ? orderedDither(pColor, x, y) : findColor(pColor);
            }
            else
            {
                Vector3f lVector; // light position in object space for proper shading
                double lVecInvLen;

                lVector.m_x = s->lightPos.m_x - pp.m_x;
                lVector.m_y = s->lightPos.m_y - pp.m_y;
                lVector.m_z = s->lightPos.m_z - pp.m_z;
                lVecInvLen = invLength(&lVector);

                // simple flat shade
                pColor[0] = s->planes[i].m_color[0] * 2.0 * lVecInvLen;
                pColor[1] = s->planes[i].m_color[1] * 2.0 * lVecInvLen;
                pColor[2] = s->planes[i].m_color[2] * 2.0 * lVecInvLen;
                clamp(pColor);

                pointColor = DITHER_ON ? orderedDither(pColor, x, y) : findColor(pColor);
            }
        }
    }

    return pointColor;
}

// ordered dither (Bayer threshold)
int orderedDither(const double *pixel, int x, int y)
{
    double newPixel[3];

    newPixel[0] = pixel[0] + thresholdMap[y % 8][x % 8];
    newPixel[1] = pixel[1] + thresholdMap[y % 8][x % 8];
    newPixel[2] = pixel[2] + thresholdMap[y % 8][x % 8];
    clamp(newPixel);

    return findColor(newPixel);
}

// standard Lambert shading
void lambertShade(const Vector3f *light, const Vector3f *normal, const double *iRGB, double *oRGB)
{
    float mul;
    float shade = dotProduct(light, normal) * 0.35f;

    if (shade < 0.0)
        shade = 0.0;

    mul = (AMBIENT + DIFFUSE * shade);

    oRGB[0] = iRGB[0] * mul;
    oRGB[1] = iRGB[1] * mul;
    oRGB[2] = iRGB[2] * mul;
}

// color quantisation using Euclidean distance
int findColor(const double *srcColor)
{
    long int nd = 196609L;
    int i, palIdx = 0;
    int startColor = GRAYSCALE_ON ? 16 : 0;
    int endColor = GRAYSCALE_ON ? 32 : 248; // last 8 colors of default VGA palette are all blacks, hence 248

    int cr = srcColor[0];
    int cg = srcColor[1];
    int cb = srcColor[2];

    // utilize full palette if it's set to grayscale and calculate luminance
    if(GRAYSCALE_PAL_ON)
    {
        endColor = 255;
        cr = cg = cb = 0.2989 * cr + 0.5870 * cg + 0.1140 * cb;
    }

    for (i = startColor; i < endColor; i++)
    {
        long int r = (long int)(cr - (double)ACTIVE_PALETTE[i][0]);
        long int g = (long int)(cg - (double)ACTIVE_PALETTE[i][1]);
        long int b = (long int)(cb - (double)ACTIVE_PALETTE[i][2]);

        // sqrt() not needed: it won't change the final evaluation
        long int d = r * r + g * g + b * b;

        if (d < nd)
        {
            nd = d;
            palIdx = i;

            // no need to look further (and waste cycles) if we hit 0; there are duplicates in VGA palette
            if (d < 1)
                return palIdx;
        }
    }

    return palIdx;
}
