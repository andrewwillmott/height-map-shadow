
// Quick test -- calls MakeHeightTestPattern for a 128^2 map,
// generates shadows over 360 degrees and saves out the corresponding
// TGAs

#define  _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include "HeightMapShadow.h"

#include "stb_image_mini.h"

namespace
{
    inline float saturate(float s)
    {
        return (s < 0) ? 0 : ((s > 1) ? 1 : s);
    }
}

void CreateNormalMap(int size, float hscale, const float hmap[], Vec3f nmap[])
{
    float dscale = hscale * (size / 2.0f);         // inter-two-pixel space is 2/w

    const float* rowPixelsHMap = hmap;
    Vec3f*       rowPixelsNMap = nmap;

    for (int iu = 0; iu < size; iu++)
        rowPixelsNMap[iu] = vl_z;

    rowPixelsHMap += size;
    rowPixelsNMap += size;

    for (int iv = 1; iv < size - 1; iv++)
    {
        rowPixelsNMap[0] = vl_z;

        for (int iu = 1; iu < size - 1; iu++)
        {
            float horiz = dscale * (rowPixelsHMap[iu - 1]    - rowPixelsHMap[iu + 1]);
            float vert  = dscale * (rowPixelsHMap[iu - size] - rowPixelsHMap[iu + size]);

            Vec3f v(norm_safe(Vec3f(horiz, vert, 1.0f)));

            rowPixelsNMap[iu] = v;
        }

        rowPixelsNMap[size - 1] = vl_z;

        rowPixelsHMap += size;
        rowPixelsNMap += size;
    }

    for (int iu = 0; iu < size; iu++)
        rowPixelsNMap[iu] = vl_z;
}

int main(int argc, const char* argv[])
{
    cHeightMapShadow hsm;
    
    const float kHeightMax = 64.0f;

    hsm.SetDimensions(128, 128, 1.0f);
    hsm.MakeHeightTestPattern(kHeightMax);
    hsm.mShadowStrength = 0.25f;
    
    std::vector<uint8_t> L8(128 * 128, 255);
    
    for (size_t i = 0, n = L8.size(); i < n; i++)
        L8[i] = uint8_t(lrintf(255.0f * hsm.mHeightMap[i] / kHeightMax));
    
    stbi_write_png("heightMap.png", 128, 128, 1, L8.data(), 0);

    // Create a normal map just for some pseudo lighting
    Vec3f normalMap[128 * 128];
    CreateNormalMap(128, kHeightMax / 128, hsm.mHeightMap.data(), normalMap);

    for (int j = 0; j < 32; j++)
    {
        float s = j / 32.0f;

        Vec3f sunDir(sinf(vl_twoPi * s), cosf(vl_twoPi * s), 1.0f);
        sunDir = norm_safe(sunDir);

        hsm.FindHeightMapShadows(sunDir);

        for (size_t i = 0, n = L8.size(); i < n; i++)
        {
            float sd = hsm.mShadowMap[i] - hsm.mHeightMap[i];

            float d = saturate(dot(sunDir, normalMap[i]));
            d = 0.2f + 0.8f * d;

            float s = hsm.ShadowScale(sd);

            uint8_t l8 = uint8_t(lrintf(255.0f * s * d));
            
            L8[i] = l8;
        }

        char fileName[32];
        snprintf(fileName, 32, "shadowMap-%02d.png", j);
        stbi_write_png(fileName, 128, 128, 1, L8.data(), 0);
    }
    
    return 0;
}

