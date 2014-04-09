//------------------------------------------------------------------------------
// Height map shadow generation
//------------------------------------------------------------------------------

#include "HeightMapShadow.h"

namespace
{
    inline float saturate(float s)
    {
        return (s < 0) ? 0 : ((s > 1) ? 1 : s);
    }
}


cHeightMapShadow::cHeightMapShadow() :
   mHeightMap(),
   mShadowMap(),
   mSamplesX(0),
   mSamplesY(0),
   mCellWidth(0),
   mInvCellWidth(0),
   mMaxDepth(kDefaultMaxUnshadowedDepth),
   mEmptyShadowMap(false),
   mShadowUpdateRect(-1, -1, 0, 0),

   mShadowStrength(kDefaultShadowStrength)
{
}

void cHeightMapShadow::SetDimensions(int samplesX, int samplesY, float cellWidth, float maxShadowDepth)
{
   mCellWidth = cellWidth;
   mInvCellWidth = 1 / cellWidth;

   // For now, to be consistent with the light map, we're going to sample texel centres rather than vertices.
   // Also, we'll include tiles < MinX() etc. for more efficient lookup.
   mSamplesX = samplesX;
   mSamplesY = samplesY;

   mMaxDepth = maxShadowDepth;

   mHeightMap .clear();
   mShadowMap .clear();

   mHeightMap .resize(mSamplesX * mSamplesY, mMaxDepth);
   mShadowMap .resize(mSamplesX * mSamplesY, mMaxDepth);
}

void cHeightMapShadow::FindHeightMapShadows(const Vec3f& sunDir)
{
   /*
      Scan order: we scan in x over columns if abs(sy) <= abs(sx)
      otherwise we scan in y over rows.
      the signs of sx and sy control which way scan we for each.
      the general algorithm is to keep track of a "crest" of shadow
      heights. The shadow crest is updated according to the current
      row or column of the height map. Then it is shifted up to
      one unit left or right and the heights adjusted downwards
      to match the location of the next row or column.
   */
   float absX = fabsf(sunDir[0]);
   float absY = fabsf(sunDir[1]);

   if (sunDir[2] <= 0 || (absX < 1e-3 && absY < 1e-3))
   {
      // no shadow...
      if (!mEmptyShadowMap)
         ClearShadowMap();
   }
   else
   {
      if (absX < absY)
         FindHeightMapShadowsRowBased(sunDir);
      else
         FindHeightMapShadowsColumnBased(sunDir);

      mEmptyShadowMap = false;
   }
}

void cHeightMapShadow::FindHeightMapShadowsRowBased(const Vec3f& sunDir)
{
   cRect updateRect(-1, -1, 0, 0);

   int w = mSamplesX;
   int h = mSamplesY;

   // the crest is the current shadow height for a particular row
   mCrest.resize(w);
   fill(mCrest.begin(), mCrest.end(), mMaxDepth);

   // scanning in y
   float crestOffset  = -sunDir[0] / sunDir[1];
   float crestDescent = mCellWidth * sunDir[2] / sunDir[1];

   // we're always marching away from the light direction.
   int beginY, endY, dY;
   if (crestDescent < 0)
   {
      beginY = 0;
      endY   = h;
      dY     = 1;
   }
   else
   {
      // flip scan direction to get a -ve crestDescent.
      beginY = h - 1;
      endY   = -1;
      dY     = -1;
      crestDescent = -crestDescent;
      crestOffset  = -crestOffset;
   }

   CL_ASSERT(crestDescent <= 0);
   CL_ASSERT(-1 <= crestOffset && crestOffset <= 1); // only shift up to a cell at a time.

   for (int y = beginY; y != endY; y += dY)
   {
      float* srcSpan = &mHeightMap.front() + y * w;
      float* dstSpan = &mShadowMap.front() + y * w;

      int firstUpdate = -1;
      int lastUpdate  = 0;

      // perform shadowing of the current row by the crest
      for (int x = 0; x < w; x++)
      {
         // if we're in light, we become the new crest.
         if (mCrest[x] < srcSpan[x])
            mCrest[x] = srcSpan[x];

         if (dstSpan[x] != mCrest[x])
         {
            dstSpan[x] = mCrest[x]; // shadow "height" at this pixel
            if (firstUpdate < 0)
               firstUpdate = x;
            lastUpdate  = x;
         }

         mCrest[x] += crestDescent; // adjust for the next row            
      }

      if (firstUpdate >= 0)
      {
         if (updateRect.top < 0)
         {
            updateRect.top    = y;
            updateRect.bottom = y;
            updateRect.left   = firstUpdate;
            updateRect.right  = lastUpdate;
         }
         else
         {
            updateRect.bottom = y;
            if (firstUpdate < updateRect.left)
               updateRect.left = firstUpdate;
            if (lastUpdate  > updateRect.right)
               updateRect.right = lastUpdate;
         }
      }

      // now shift the crest by crestOffset for the next row.   
      // if crestOffset is positive, we must scroll the crest
      // backwards by that amount, and vice, versa.
      if (crestOffset > 0)
      {
         for (int x = 0; x < w - 1; x++)
            mCrest[x] = lerp(mCrest[x], mCrest[x + 1], crestOffset);
         // extrapolate to find last height.
         mCrest[w - 1] = 2 * mCrest[w - 2] - mCrest[w - 3];
         // Sometimes it works better to assume external heights = 0
         //mCrest[w - 1] *= (1 - crestOffset); // lerp(mCrest[w - 1], 0, crestOffset)
      }
      else
      {
         for (int x = w - 1; x > 0; x--)
            mCrest[x] = lerp(mCrest[x], mCrest[x - 1], -crestOffset);
         // extrapolate to find first height.
         mCrest[0] = 2 * mCrest[1] - mCrest[2];
          // Sometimes it works better to assume external heights = 0
         //mCrest[0] *= (1 + crestOffset); // lerp(mCrest[0], 0, -crestOffset)
      }
   }

   mShadowUpdateRect = updateRect;
}


void cHeightMapShadow::FindHeightMapShadowsColumnBased(const Vec3f& sunDir)
{
   cRect updateRect(-1, -1, 0, 0);

   int w = mSamplesX;
   int h = mSamplesY;

   // the crest is the current shadow height for a particular column
   mCrest.resize(h);
   fill(mCrest.begin(), mCrest.end(), mMaxDepth);

   // scanning in x
   float crestOffset  = -sunDir[1] / sunDir[0];
   float crestDescent = mCellWidth * sunDir[2] / sunDir[0];

   // we're always marching away from the light direction.
   int beginX, endX, dX;
   if (crestDescent < 0)
   {
      beginX = 0;
      endX   = w;
      dX     = 1;
   }
   else
   {
      // flip scan direction to get a -ve crestDescent
      beginX = w - 1;
      endX   = -1;
      dX     = -1;
      crestDescent = -crestDescent;
      crestOffset  = -crestOffset;
   }

   CL_ASSERT(crestDescent <= 0);
   CL_ASSERT(-1 <= crestOffset && crestOffset <= 1); // only shift up to a cell at a time.

   for (int x = beginX; x != endX; x += dX)
   {
      float* srcSpan = &mHeightMap.front() + x;
      float* dstSpan = &mShadowMap.front() + x;

      int firstUpdate = -1;
      int lastUpdate  = 0;

      // perform shadowing of the current row by the crest
      for (int y = 0; y < h; y++)
      {
         // if we're in light, we become the new crest.
         if (mCrest[y] < srcSpan[y * w])
            mCrest[y] = srcSpan[y * w];

         if (dstSpan[y * w] != mCrest[y])
         {
            dstSpan[y * w] = mCrest[y]; // shadow "height" at this pixel
            if (firstUpdate < 0)
               firstUpdate = y;
            lastUpdate  = y;
         }

         mCrest[y] += crestDescent; // adjust for the next row            
      }

      if (firstUpdate >= 0)
      {
         // first update to rectangle?
         if (updateRect.left < 0)
         {
            updateRect.top    = firstUpdate;
            updateRect.bottom = lastUpdate;
            updateRect.left   = x;
            updateRect.right  = x;
         }
         else
         {
            updateRect.right = x;
            if (firstUpdate < updateRect.top)
               updateRect.top = firstUpdate;
            if (lastUpdate  > updateRect.bottom)
               updateRect.bottom = lastUpdate;
         }
      }

      // now shift the crest by crestOffset for the next row.   
      // if crestOffset is positive, we must scroll the crest
      // backwards by that amount, and vice versa.
      if (crestOffset > 0) 
      {
         for (int y = 0; y < h - 1; y++)
            mCrest[y] = lerp(mCrest[y], mCrest[y + 1], crestOffset);
         // extrapolate to find last height.
         mCrest[h - 1] = 2 * mCrest[h - 2] - mCrest[h - 3];
          // Sometimes it works better to assume external heights = 0
         //mCrest[h - 1] *= (1 - crestOffset); // lerp(mCrest[h - 1], 0, crestOffset);
      }
      else
      {
         for (int y = h - 1; y > 0; y--)
            mCrest[y] = lerp(mCrest[y], mCrest[y - 1], -crestOffset);
         // extrapolate to find first height.
         mCrest[0] = 2 * mCrest[1] - mCrest[2];
          // Sometimes it works better to assume external heights = 0
         //mCrest[0] *= (1 + crestOffset); // lerp(mCrest[0], 0, -crestOffset);
      }
   }

   mShadowUpdateRect = updateRect;
}

void cHeightMapShadow::ClearShadowMap()
{
   fill(mShadowMap.begin(), mShadowMap.end(), mMaxDepth);
   mEmptyShadowMap = true;
}

void cHeightMapShadow::MakeHeightTestPattern(float hmax)
{
    int w = mSamplesX;
    int h = mSamplesY;
    
    if (hmax < 0.0f)
        hmax = mCellWidth * 4.0f;
    
    float s = mCellWidth * vl_pi / 32.0f;
    
    for (int y = 0; y < h; y++)
    {
        float* dstSpan = &mHeightMap.front() + y * w;
        
        for (int x = 0; x < w; x++)
            dstSpan[x] = (sqr(sinf(s * x)) * sqr(sinf(s * y))) * hmax;
    }
}

void cHeightMapShadow::MakeShadowTestPattern()
{
   int w = mSamplesX;
   int h = mSamplesY;

   for (int y = 0; y < h; y++)
   {
      float* dstSpan = &mShadowMap.front() + y * w;

      for (int x = 0; x < w; x++)
         dstSpan[x] = x / float(mSamplesX) * y / float(mSamplesY);
   }
}


#ifdef TEST_HMS

// Quick test -- calls MakeHeightTestPattern for a 128^2 map,
// generates shadows over 360 degrees and saves out the corresponding
// TGAs
#include "targa.h"

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
        L8[i] = lrintf(255.0f * hsm.mHeightMap[i] / kHeightMax);
    
    tga_image tga;
    init_tga_image(&tga, (uint8_t*) L8.data(), 128, 128, 8);
    tga.image_type = TGA_IMAGE_TYPE_MONO;
    tga.image_descriptor &= ~TGA_T_TO_B_BIT;
    tga_write("heightMap.tga", &tga);

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

            uint8_t l8 = lrintf(255.0f * s * d);
            
            L8[i] = l8;
        }

        char fileName[32];
        snprintf(fileName, 32, "shadowMap-%02d.tga", j);
        tga_write(fileName, &tga);
    }
    
    return 0;
}

#endif
