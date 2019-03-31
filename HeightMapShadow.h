//------------------------------------------------------------------------------
// Height map shadow generation
//------------------------------------------------------------------------------

#ifndef HEIGHT_MAP_SHADOW_H
#define HEIGHT_MAP_SHADOW_H

#include "VL234f.h"
#include <algorithm>
#include <vector>

const float  kDefaultShadowStrength     = 1;        // strength modifier for hill shadows.
const float  kDefaultMaxUnshadowedDepth = -1000;    // below this depth we're always in shadow

struct cRect
{
    int left;
    int top;
    int right;
    int bottom;
    
    cRect(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}
};

class cHeightMapShadow
/// Utility class to calculate shadows cast by a height map.
{
public:
    // Data
    std::vector<float>   mHeightMap;          ///< the shadow-casting height map. holds sample heights, has mSamplesX * mSamplesY values.
    std::vector<float>   mShadowMap;          ///< the derived shadow map. contains the per-vertex height at which you cross into shadow.

    int             mSamplesX;           ///< height map samples in X direction
    int             mSamplesY;           ///< height map samples in Y direction
    float           mCellWidth;          ///< width of a heightmap cell in world coordinates.
    float           mInvCellWidth;       ///< reciprocal of mCellWidth

    float           mMaxDepth;           ///< minimum for shadow map.
    bool            mEmptyShadowMap;     ///< shadow map is cleared to mMaxDepth
    cRect           mShadowUpdateRect;   ///< those samples in the shadow map whose heights were updated on the last FindHeightMapShadows
    
    float           mShadowStrength;     ///< strength modifier for hill shadows.
    
    cHeightMapShadow();
    
    // Methods
    void SetDimensions(int samplesX, int samplesY, float cellWidth, float maxShadowDepth = kDefaultMaxUnshadowedDepth);
    ///< Set up bounds/scale of maps.
    void FindHeightMapShadows(const Vec3f& sunDir);
    ///< Calculate mShadowMap from mHeightMap according to sunDir, which is taken to point towards the sun. */
    void ClearShadowMap();
    ///< Clears shadow map to max depth.

    void MakeHeightTestPattern(float hmax = -1.0f);
    ///< Clears height map to test pattern.
    void MakeShadowTestPattern();
    ///< Clears shadow map to test pattern.
    
    float ShadowScale(float shadowDepth);
    ///< Returns 0-1 scalar shadow strength for given shadowdepth
    Vec3f BendShadowedNormal(const Vec3f& normal, const Vec3f& skylightDirection, float shadowDepth);
    ///< Bends normal towards the major skylight direction according to shadowDepth, so as to simulate sun shadowing.

    bool LocationIsInBounds(float x, float y);
    ///< Returns true if this is a valid height map location.
    
    float ShadowDepth(const Vec2f& p);
    ///< Depth of shadow at the given ground point. (Positive = in shadow.)
    float ShadowDepth(int x, int y);
    ///< Depth of shadow at the given ground cell
    
    float ObjectShadowDepth(const Vec3f& p);
    ///< Depth of shadow at arbitrary point p. (E.g., useful for heightmap -> object shadows.)

    float& HeightMap(int x, int y);
    ///< Height map value at given cell
    float& ShadowMap (int x, int y);
    ///< Shadow map value at given cell

protected:
    void FindHeightMapShadowsRowBased   (const Vec3f& sunDir);
    void FindHeightMapShadowsColumnBased(const Vec3f& sunDir);
    
    std::vector<float> mCrest;              ///< temporary used by FindHeightMapShadows*()
};



inline float cHeightMapShadow::ShadowScale(float shadowDepth)
{
    return 1.0f / (1.0f + std::max(shadowDepth, 0.0f) * mShadowStrength);
}

inline Vec3f cHeightMapShadow::BendShadowedNormal(const Vec3f& normal, const Vec3f& skylightDirection, float shadowDepth)
{
   Vec3f bentNormal = normal + skylightDirection * (shadowDepth * mShadowStrength);
   float n2 = sqrlen(bentNormal);
   
   if (n2 > 0)
      bentNormal /= sqrtf(n2);

   return bentNormal;
}

inline bool cHeightMapShadow::LocationIsInBounds(float x, float y)
{
   float cx = x * mInvCellWidth;
   float cy = y * mInvCellWidth;

   return (cx >= 0) && (cy >= 0) && (cx <= mSamplesX - 1) && (cy <= mSamplesY - 1);
}

inline float cHeightMapShadow::ShadowDepth(const Vec2f& p)
{
   int vx = int(p[0] * mInvCellWidth);
   int vy = int(p[1] * mInvCellWidth);
   int idx = vx + vy * mSamplesX;

   return mShadowMap [idx]
        - mHeightMap[idx];
}

inline float cHeightMapShadow::ShadowDepth(int x, int y)
{
   int idx = x + y * mSamplesX;

   return mShadowMap [idx]
        - mHeightMap[idx];
}

inline float cHeightMapShadow::ObjectShadowDepth(const Vec3f& p)
{
   int vx = int(p[0] * mInvCellWidth);
   int vy = int(p[1] * mInvCellWidth);

   return mShadowMap[vx + vy * mSamplesX] - p[2];
}

inline float& cHeightMapShadow::HeightMap(int vx, int vy)
{
   return mHeightMap[vx + vy * mSamplesX];
}

inline float& cHeightMapShadow::ShadowMap(int vx, int vy)
{
   return mShadowMap[vx + vy * mSamplesX];
}


#endif
