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

	struct
	{
        struct DensityProfile
        {
            struct 
            {
                float width;
                float exp_term;
                float exp_scale;
                float linear_term;
                float constant_term;
            }layers[2];
        };

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

};