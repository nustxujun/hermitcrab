#include "common.hlsl"



struct DensityProfileLayer
{
	float width;
	float exp_term;
	float exp_scale;
	float linear_term;
	float constant_term;
	float3 unused;
};

struct DensityProfile
{
	DensityProfileLayer layers[2];
};


struct AtomsphereParameters
{
	float3 solar_irradiance;
  	float sun_angular_radius;

	float3 rayleigh_scattering;
	float bottom_radius;

	float3 mie_scattering;
	float top_radius;

	float3 mie_extinction;
	float mie_phase_function_g;

	float3 absorption_extinction;
	float mu_s_min;

	float3 ground_albedo;
	float unused;

  	DensityProfile rayleigh_density;
  	DensityProfile mie_density;
  	DensityProfile absorption_density;
};

cbuffer AtomsphereConstants
{
	AtomsphereParameters parameters;
	float2 texture_size;
};


float clampCosine(float mu){
	return(clamp(mu, -1.0f, 1.0f));
}


float getLayerDensity(DensityProfileLayer layer, float altitude)
{
	float density = layer.exp_term * exp( layer.exp_scale * altitude ) + layer.linear_term * altitude + layer.constant_term;
	return clamp(density, 0, 1.0f);
}

float getProfileDensity(DensityProfile profile, float altitude)
{
	return altitude < profile.layers[0].width ? getLayerDensity(profile.layers[0], altitude) : getLayerDensity(profile.layers[1], altitude);
}

float distanceToAtmosphereTop(AtomsphereParameters params, float r, float mu)
{
	float d = r * r * (mu * mu - 1.0f) + params.top_radius * params.top_radius;
	return max(-r * mu + sqrt( max(d, 0)), 0);
}

float calculateOpticalDepthToAtmosphereTop(AtomsphereParameters params, DensityProfile profile, float r, float mu)
{
	const int SAMPLE_COUNT = 500;
	float dx = distanceToAtmosphereTop(params, r, mu) / float(SAMPLE_COUNT);
	float result = 0;
	for (int i = 0; i <= SAMPLE_COUNT; ++i)
	{
		float d_i = float(i) * dx;
		float r_i = sqrt(d_i * d_i + 2.0f * r * mu * d_i + r * r );
		float y_i = getProfileDensity(profile, r_i - params.bottom_radius);
		float weight_i = i == 0 || i == SAMPLE_COUNT ? 0.5f : 1.0f; 
		result += y_i * weight_i * dx;
	}
	return result;
}

float3 calculateTransmittanceToAtmosphereTop(AtomsphereParameters params, float r, float mu)
{
  return exp(-(
      params.rayleigh_scattering *
          calculateOpticalDepthToAtmosphereTop(
              params, params.rayleigh_density, r, mu) +
      params.mie_extinction *
          calculateOpticalDepthToAtmosphereTop(
              params, params.mie_density, r, mu) +
      params.absorption_extinction *
          calculateOpticalDepthToAtmosphereTop(
              params, params.absorption_density, r, mu)));
}

float getUnitRangeFromTextureCoord(float u, float size)
{
	return (( u - 0.5f / size) / (1.0f - 1.0f / size));
}

void getRMUFromTransmittanceTextureCoord(AtomsphereParameters params,float2 uv, out float r,out float mu)
{
	float x_mu = getUnitRangeFromTextureCoord(uv.x, texture_size.x);
	float x_r = getUnitRangeFromTextureCoord(uv.y, texture_size.y);

	float bb = params.bottom_radius * params.bottom_radius;

	float H = sqrt(params.top_radius * params.top_radius - bb);
	float rho = H * x_r;
	r = sqrt(rho * rho + bb );
	float d_min = params.top_radius - r;
	float d_max = rho + H;
	float d = d_min + x_mu * (d_max - d_min);
	mu = d == 0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0f * r * d);
	mu = clampCosine(mu);
}

float3 calculateTransmittanceToAtmosphereTopTexture(AtomsphereParameters params, float2 uv)
{
	float r, mu;
	getRMUFromTransmittanceTextureCoord(params, uv / texture_size, r, mu);
	return calculateTransmittanceToAtmosphereTop(params, r, mu);
}


half4 precomputeTransmittanceToAtmosphereTop(QuadInput input) :SV_Target
{
	return half4(calculateTransmittanceToAtmosphereTopTexture(parameters, input.uv), 1);
}


