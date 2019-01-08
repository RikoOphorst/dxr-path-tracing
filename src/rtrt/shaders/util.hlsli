#ifndef UTIL
#define UTIL

//
// Random number generation
//------------------------------------------------------------------------------------------------------
// From: http://intro-to-dxr.cwyman.org/
uint initRand(uint val0, uint val1, uint backoff = 16)
{
  uint v0 = val0, v1 = val1, s0 = 0;

  [unroll]
  for (uint n = 0; n < backoff; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }
  return v0;
}

//------------------------------------------------------------------------------------------------------
// From: http://intro-to-dxr.cwyman.org/
// Takes a seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
  s = (1664525u * s + 1013904223u);
  return float(s & 0x00FFFFFF) / float(0x01000000);
}

//
// Cosine weighted hemisphere sampling
//------------------------------------------------------------------------------------------------------
// From: http://intro-to-dxr.cwyman.org/
float3 GetPerpendicularVector(float3 u)
{
  float3 a = abs(u);
  uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
  uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
  uint zm = 1 ^ (xm | ym);
  return cross(u, float3(xm, ym, zm));
}

//------------------------------------------------------------------------------------------------------
// From: http://intro-to-dxr.cwyman.org/
float3 CosineWeightedHemisphereSample(inout uint seed, float3 normal)
{
  float2 random = float2(nextRand(seed), nextRand(seed));

  float3 bitangent = GetPerpendicularVector(normal);
  float3 tangent = cross(bitangent, normal);
  float r = sqrt(random.x);
  float phi = 2.0f * 3.14159265f * random.y;

  return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(1 - random.x);
}

// Calculates barycentrical interpolation factors based on actual barycentrics
float3 CalculateBarycentricalInterpolationFactors(in float2 barycentrics)
{
  return float3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
}

// b == output from CalculateBarycentricalInterpolationFactors()
float1 BarycentricInterpolation(in float1 a0, in float1 a1, in float1 a2, in float3 b) {
  return b.x * a0 + b.y * a1 + b.z * a2;
}

// b == output from CalculateBarycentricalInterpolationFactors()
float2 BarycentricInterpolation(in float2 a0, in float2 a1, in float2 a2, in float3 b) {
  return b.x * a0 + b.y * a1 + b.z * a2;
}

// b == output from CalculateBarycentricalInterpolationFactors()
float3 BarycentricInterpolation(in float3 a0, in float3 a1, in float3 a2, in float3 b) {
  return b.x * a0 + b.y * a1 + b.z * a2;
}

// b == output from CalculateBarycentricalInterpolationFactors()
float4 BarycentricInterpolation(in float4 a0, in float4 a1, in float4 a2, in float3 b) {
  return b.x * a0 + b.y * a1 + b.z * a2;
}

//------------------------------------------------------------------------------------------------------
inline float3 RandomPointInUnitDisk(inout uint seed)
{
  float3 p = float3(2.0f, 2.0f, 2.0f);

  while (dot(p, p) > 1.0f)
  {
    p = float3(nextRand(seed) * 2.0f - 1.0f, nextRand(seed) * 2.0f - 1.0f, 0.0f);
  }

  return p;
}

//------------------------------------------------------------------------------------------------------
inline float3 RandomPointInUnitSphere(inout uint seed)
{
  float3 p = float3(2.0f, 2.0f, 2.0f);

  while (length(p) > 1.0f)
  {
    p = float3(nextRand(seed) * 2.0f - 1.0f, nextRand(seed) * 2.0f - 1.0f, nextRand(seed) * 2.0f - 1.0f);
  }

  return p;
}

//------------------------------------------------------------------------------------------------------
inline bool srefract(in float3 v, in float3 n, in float ni_over_nt, out float3 refracted)
{
  float dt = dot(v, n);
  float discriminant = 1.0f - ni_over_nt * ni_over_nt * (1 - dt * dt);

  if (discriminant > 0)
  {
    refracted = ni_over_nt * (v - n * dt) - n * sqrt(discriminant);
    return true;
  }

  return false;
}

//------------------------------------------------------------------------------------------------------
inline float schlick(in float cosine, in float index_of_refraction)
{
  float r0 = (1 - index_of_refraction) / (1 + index_of_refraction);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}

#endif