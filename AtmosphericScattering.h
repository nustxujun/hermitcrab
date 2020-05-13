#pragma once
#include "Common.h"
#include "RenderGraph.h"
#include "Quad.h"
#include "ImguiOverlay.h"

class AtmosphericScattering
{
public:
	AtmosphericScattering();

	void execute(RenderGraph& graph);

    Renderer::Resource::Ref getTransmittanceToTop();
private:
    void initAtmospehre();
private:
	Quad::Ptr mQuad;
	Renderer::ConstantBuffer::Ptr mAtmosphereConsts;
	Renderer::Resource::Ref mTransmittanceToAtmosphereTop;
	bool mRecompute = true;

    struct DensityProfileLayer
    {
        float width = 0;
        float exp_term = 0;
        float exp_scale = 0;
        float linear_term = 0;

        float constant_term = 0;
        float3 unused = {};
    };

	struct 
	{
        struct 
        {
            struct DensityProfile
            {
                DensityProfileLayer layers[2];
            };

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
        } params;

        float2 texture_size;
	}mAtmosphereParams;

    ImGuiOverlay::ImGuiObject* mSettings;

    struct
    {
        std::map<std::string, std::vector<double>> curves;

        struct DensityProfiles
        {
            std::string name;
            std::vector<DensityProfileLayer> curve;
            int selected = 0 ;
        };

        std::vector<DensityProfiles> densities;

        double max_sun_zenith_angle;
        double length_unit_in_meters;
        bool combine_scattering_textures;
        bool half_precision;
    }mAtmosphereSettings;

};