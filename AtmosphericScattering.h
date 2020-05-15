#pragma once

#include "RenderGraph.h"
#include "Quad.h"
#include "ImguiOverlay.h"

class AtmosphericScattering
{
public:
	AtmosphericScattering();

	void execute(RenderGraph& graph);

private:
	Quad::Ptr mQuad;
	Renderer::ConstantBuffer::Ptr mAtmosphereConsts;
	Renderer::Resource::Ref mTransmittanceToAtmosphereTop;
	bool mRecompute = true;

	struct DensityProfileLayer
	{
		float width;
		float exp_term;
		float exp_scale;
		float linear_term;
		float constant_term;
	};

	struct DensityProfile
	{
		DensityProfileLayer layers[2];
	};

	struct
	{
        float3 solar_irradiance;
        float sun_angular_radius;
        float bottom_radius;
        float top_radius;
        DensityProfile rayleigh_density;
        float3 rayleigh_scattering;
        DensityProfile mie_density;
        float3 mie_scattering;
        float3 mie_extinction;
        float mie_phase_function_g;
        DensityProfile absorption_density;
        float3 absorption_extinction;
        float3 ground_albedo;
        float mu_s_min;
	}mAtmosphereParams;

    ImGuiOverlay::ImGuiObject* mSettings;
	struct
	{
		std::vector<double> wavelengths;
		std::vector<double> solar_irradiance;
		
		std::vector<DensityProfileLayer> rayleigh_density;

	}
	mSettingParams;

};